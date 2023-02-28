// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ipc_utils.h"

#include <utility>

#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/frame.mojom.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"

namespace content {

namespace {

// Validates that |received_token| is non-null iff associated with a blob: URL.
bool VerifyBlobToken(
    int process_id,
    const mojo::PendingRemote<blink::mojom::BlobURLToken>& received_token,
    const GURL& received_url) {
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);

  if (received_token.is_valid()) {
    if (!received_url.SchemeIsBlob()) {
      bad_message::ReceivedBadMessage(
          process_id, bad_message::BLOB_URL_TOKEN_FOR_NON_BLOB_URL);
      return false;
    }
  }

  return true;
}

bool VerifyInitiatorOrigin(int process_id,
                           const url::Origin& initiator_origin) {
  // TODO(acolwell, nasko): https://crbug.com/1029092: Ensure the precursor of
  // opaque origins matches the origin lock.  One known problematic case are
  // reloads initiated from error pages - see the following
  // RenderFrameHostManagerTest tests:
  // 1. ErrorPageNavigationReload:
  //    - renderer origin lock = chrome-error://chromewebdata/
  //    - precursor of initiator origin = http://127.0.0.1:.../
  // 2. ErrorPageNavigationReload_InSubframe_BlockedByClient
  //    - renderer origin lock = http://b.com:.../
  //    - precursor of initiator origin = http://c.com:.../
  if (initiator_origin.opaque())
    return true;

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(process_id, initiator_origin)) {
    bad_message::ReceivedBadMessage(process_id,
                                    bad_message::INVALID_INITIATOR_ORIGIN);
    return false;
  }

  return true;
}

bool VerifyHasStorageAccess(
    const RenderFrameHostImpl& current_rfh,
    blink::LocalFrameToken* initiator_frame_token,
    const blink::mojom::CommonNavigationParams& common_params) {
  if (!common_params.has_storage_access) {
    return true;
  }

  // The initiator origin must be provided, and must be same-origin with the
  // request URL.
  if (!common_params.initiator_origin.has_value() ||
      !common_params.initiator_origin.value().IsSameOriginWith(
          common_params.url)) {
    return false;
  }

  // The initiator's frame token must be provided and must be equal to the
  // current frame token.
  if (!initiator_frame_token ||
      *initiator_frame_token != current_rfh.GetFrameToken()) {
    return false;
  }

  return true;
}

}  // namespace

bool VerifyDownloadUrlParams(SiteInstance* site_instance,
                             const blink::mojom::DownloadURLParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(site_instance);
  RenderProcessHost* process = site_instance->GetProcess();
  int process_id = process->GetID();

  // Verifies |params.blob_url_token| is appropriately set.
  if (!VerifyBlobToken(process_id, params.blob_url_token, params.url))
    return false;

  // Verify |params.initiator_origin|.
  if (params.initiator_origin &&
      !VerifyInitiatorOrigin(process_id, *params.initiator_origin))
    return false;

  // If |params.url| is not set, this must be a large data URL being passed
  // through |params.data_url_blob|.
  if (!params.url.is_valid() && !params.data_url_blob.is_valid())
    return false;

  // Verification succeeded.
  return true;
}

bool VerifyOpenURLParams(RenderFrameHostImpl* current_rfh,
                         SiteInstance* site_instance,
                         const blink::mojom::OpenURLParamsPtr& params,
                         GURL* out_validated_url,
                         scoped_refptr<network::SharedURLLoaderFactory>*
                             out_blob_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(current_rfh);
  DCHECK(site_instance);
  DCHECK(out_validated_url);
  DCHECK(out_blob_url_loader_factory);
  RenderProcessHost* process = site_instance->GetProcess();
  int process_id = process->GetID();

  // Verify |params.url| and populate |out_validated_url|.
  *out_validated_url = params->url;
  process->FilterURL(false, out_validated_url);

  // Verify |params.blob_url_token| and populate |out_blob_url_loader_factory|.
  if (!VerifyBlobToken(process_id, params->blob_url_token, params->url))
    return false;

  if (params->blob_url_token.is_valid()) {
    *out_blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            site_instance->GetBrowserContext()->GetStoragePartition(
                site_instance),
            std::move(params->blob_url_token));
  }

  // Verify |params.post_body|.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanReadRequestBody(site_instance, params->post_body)) {
    bad_message::ReceivedBadMessage(process,
                                    bad_message::ILLEGAL_UPLOAD_PARAMS);
    return false;
  }

  // Verify |params.initiator_origin|.
  if (!VerifyInitiatorOrigin(process_id, params->initiator_origin))
    return false;

  // Verify that the initiator frame can navigate `current_rfh`.
  if (!VerifyNavigationInitiator(current_rfh, params->initiator_frame_token,
                                 process_id)) {
    return false;
  }

  if (params->is_container_initiated) {
    if (!current_rfh->GetParent() ||
        (current_rfh->GetParent()->GetFrameToken() !=
         params->initiator_frame_token)) {
      mojo::ReportBadMessage(
          "container initiated navigation from non-parent process");
      return false;
    }
  }

  // Verification succeeded.
  return true;
}

bool VerifyBeginNavigationCommonParams(
    const RenderFrameHostImpl& current_rfh,
    SiteInstance* site_instance,
    blink::LocalFrameToken* initiator_frame_token,
    blink::mojom::CommonNavigationParams* common_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(site_instance);
  DCHECK(common_params);
  RenderProcessHost* process = site_instance->GetProcess();
  int process_id = process->GetID();

  // Verify (and possibly rewrite) |url|.
  process->FilterURL(false, &common_params->url);
  if (common_params->url.SchemeIs(kChromeErrorScheme)) {
    mojo::ReportBadMessage("Renderer cannot request error page URLs directly");
    return false;
  }

  // Verify |post_data|.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanReadRequestBody(site_instance, common_params->post_data)) {
    bad_message::ReceivedBadMessage(process,
                                    bad_message::ILLEGAL_UPLOAD_PARAMS);
    return false;
  }

  // Verify |transition| is webby.
  if (!PageTransitionIsWebTriggerable(
          ui::PageTransitionFromInt(common_params->transition))) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFHI_BEGIN_NAVIGATION_NON_WEBBY_TRANSITION);
    return false;
  }

  // Verify |initiator_origin|.
  if (!common_params->initiator_origin.has_value()) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFHI_BEGIN_NAVIGATION_MISSING_INITIATOR_ORIGIN);
    return false;
  }
  if (!VerifyInitiatorOrigin(process_id,
                             common_params->initiator_origin.value())) {
    return false;
  }

  // Verify |base_url_for_data_url|.
  if (!common_params->base_url_for_data_url.is_empty()) {
    // Kills the process. http://crbug.com/726142
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_BASE_URL_FOR_DATA_URL_SPECIFIED);
    return false;
  }

  // Asynchronous (browser-controlled, but) renderer-initiated navigations can
  // not be same-document. Allowing this incorrectly could have us try to
  // navigate an existing document to a different site.
  if (NavigationTypeUtils::IsSameDocument(common_params->navigation_type))
    return false;

  // Verify |has_storage_access|. This corresponds to some of the changes to
  // "create navigation params by fetching" in the Storage Access API spec:
  // https://privacycg.github.io/storage-access/#navigation
  if (!VerifyHasStorageAccess(current_rfh, initiator_frame_token,
                              *common_params)) {
    return false;
  }

  // Verification succeeded.
  return true;
}

bool VerifyNavigationInitiator(
    RenderFrameHostImpl* current_rfh,
    const absl::optional<blink::LocalFrameToken>& initiator_frame_token,
    int initiator_process_id) {
  // Verify that a frame inside a fenced frame cannot navigate its ancestors,
  // unless the frame being navigated is the outermost main frame.
  if (current_rfh->IsOutermostMainFrame())
    return true;

  if (!initiator_frame_token)
    return true;

  RenderFrameHostImpl* initiator_render_frame_host =
      RenderFrameHostImpl::FromFrameToken(initiator_process_id,
                                          initiator_frame_token.value());
  if (!initiator_render_frame_host)
    return true;

  // Verify that a frame cannot navigate a frame with a different fenced frame
  // nonce, unless the navigating frame is a fenced frame root and its owner
  // frame has the same fenced frame nonce as the initiator frame (e.g. in a
  // A(A1,A2(FF)) setup, A, A1, and A2 are all allowed to navigate FF).
  absl::optional<base::UnguessableToken> initiator_fenced_frame_nonce =
      initiator_render_frame_host->frame_tree_node()->GetFencedFrameNonce();
  if (initiator_fenced_frame_nonce !=
      current_rfh->frame_tree_node()->GetFencedFrameNonce()) {
    if (!current_rfh->IsFencedFrameRoot() ||
        current_rfh->frame_tree_node()
                ->GetParentOrOuterDocument()
                ->frame_tree_node()
                ->GetFencedFrameNonce() != initiator_fenced_frame_nonce) {
      mojo::ReportBadMessage(
          "The fenced frame nonces of initiator and current frame don't match, "
          "nor is the current frame a fenced frame root whose owner frame has "
          "the same fenced frame nonce as the initiator frame.");
      return false;
    }
  }

  if (!initiator_render_frame_host->IsNestedWithinFencedFrame())
    return true;

  FrameTreeNode* node = initiator_render_frame_host->frame_tree_node();
  if (node == current_rfh->frame_tree_node())
    return true;

  while (node) {
    node = node->parent() ? node->parent()->frame_tree_node() : nullptr;

    if (node == current_rfh->frame_tree_node()) {
      mojo::ReportBadMessage(
          "A frame in a fenced frame tree cannot navigate an ancestor frame.");
      return false;
    }
  }

  return true;
}

}  // namespace content
