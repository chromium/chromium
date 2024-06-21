// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ipc_utils.h"

#include <optional>
#include <utility>

#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/features.h"
#include "content/common/frame.mojom.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/system/message_pipe.h"
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

bool VerifyInitiatorOrigin(
    int process_id,
    const url::Origin& initiator_origin,
    const RenderFrameHostImpl* current_rfh = nullptr,
    GURL* navigation_url = nullptr,
    std::optional<blink::LocalFrameToken>* initiator_frame_token = nullptr) {
  // TODO(crbug.com/40109437): Ideally, origin verification should be performed
  // even if `initiator_origin` is opaque, to ensure that the precursor origin
  // matches the process lock. However, there are a couple of cases where this
  // doesn't yet work, which are documented and skipped below.
  if (initiator_origin.opaque()) {
    // TODO(alexmos): This used to allow all opaque origins; this behavior is
    // now behind a kill switch and should be removed once the rollout in M128
    // is complete.
    if (!base::FeatureList::IsEnabled(
            features::kAdditionalOpaqueOriginEnforcements)) {
      return true;
    }

    // Reloads initiated from error pages may currently lead to a precursor
    // mismatch, since the error page loads with an opaque origin with the
    // original URL's origin as its precursor, which may not match the error
    // page's process lock. This is seen in the following
    // RenderFrameHostManagerTest tests:
    // 1. ErrorPageNavigationReload:
    //    - renderer origin lock = chrome-error://chromewebdata/
    //    - precursor of initiator origin = http://127.0.0.1:.../
    // 2. ErrorPageNavigationReload_InSubframe_BlockedByClient
    //    - renderer origin lock = http://b.com:.../
    //    - precursor of initiator origin = http://c.com:.../
    if (current_rfh && current_rfh->IsErrorDocument()) {
      return true;
    }

    // Certain (e.g., data:) navigations in subframes of MHTML documents may
    // have precursor origins that do not match the process lock of the MHTML
    // document. This is seen in NavigationMhtmlBrowserTest.DataIframe, where:
    //   - renderer origin lock = { file:/// sandboxed }
    //   - precursor of initiator origin = http://8.8.8.8/
    // Note that RenderFrameHostImpl::CanCommitOriginAndUrl() similarly allows
    // such navigations to commit, and it also ensures that they can only commit
    // in the main frame MHTML document's process.
    if (current_rfh && current_rfh->IsMhtmlSubframe()) {
      return true;
    }
  }

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->HostsOrigin(process_id, initiator_origin)) {
    if (navigation_url) {
      static auto* const navigation_url_key =
          base::debug::AllocateCrashKeyString(
              "navigation_url", base::debug::CrashKeySize::Size256);
      base::debug::SetCrashKeyString(
          navigation_url_key,
          navigation_url->DeprecatedGetOriginAsURL().spec());
    }
    if (initiator_frame_token && initiator_frame_token->has_value()) {
      if (RenderFrameHostImpl* initiator_render_frame_host =
              RenderFrameHostImpl::FromFrameToken(
                  process_id, initiator_frame_token->value())) {
        static auto* const initiator_rfh_origin_key =
            base::debug::AllocateCrashKeyString(
                "initiator_rfh_origin", base::debug::CrashKeySize::Size256);
        base::debug::SetCrashKeyString(
            initiator_rfh_origin_key,
            initiator_render_frame_host->GetLastCommittedOrigin()
                .GetDebugString());
      }
    }

    if (current_rfh) {
      auto bool_to_crash_key = [](bool b) { return b ? "true" : "false"; };
      static auto* const is_main_frame_key =
          base::debug::AllocateCrashKeyString(
              "is_main_frame", base::debug::CrashKeySize::Size32);
      base::debug::SetCrashKeyString(
          is_main_frame_key, bool_to_crash_key(current_rfh->is_main_frame()));

      static auto* const is_outermost_frame_key =
          base::debug::AllocateCrashKeyString(
              "is_outermost_frame", base::debug::CrashKeySize::Size32);
      base::debug::SetCrashKeyString(
          is_outermost_frame_key,
          bool_to_crash_key(current_rfh->IsOutermostMainFrame()));

      static auto* const is_on_initial_empty_document_key =
          base::debug::AllocateCrashKeyString(
              "is_on_initial_empty_doc", base::debug::CrashKeySize::Size32);
      base::debug::SetCrashKeyString(
          is_on_initial_empty_document_key,
          bool_to_crash_key(
              current_rfh->frame_tree_node()->is_on_initial_empty_document()));

      static auto* const last_committed_origin_key =
          base::debug::AllocateCrashKeyString(
              "last_committed_origin", base::debug::CrashKeySize::Size256);
      base::debug::SetCrashKeyString(
          last_committed_origin_key,
          current_rfh->GetLastCommittedOrigin().GetDebugString());

      if (current_rfh->GetParentOrOuterDocumentOrEmbedder()) {
        static auto* const parent_etc_origin_key =
            base::debug::AllocateCrashKeyString(
                "parent_etc_origin", base::debug::CrashKeySize::Size256);
        base::debug::SetCrashKeyString(
            parent_etc_origin_key,
            current_rfh->GetParentOrOuterDocumentOrEmbedder()
                ->GetLastCommittedOrigin()
                .GetDebugString());
      }

      if (FrameTreeNode* opener = current_rfh->frame_tree_node()->opener()) {
        static auto* const opener_origin_key =
            base::debug::AllocateCrashKeyString(
                "opener_origin", base::debug::CrashKeySize::Size256);
        base::debug::SetCrashKeyString(opener_origin_key,
                                       opener->current_frame_host()
                                           ->GetLastCommittedOrigin()
                                           .GetDebugString());
      }
    }

    bad_message::ReceivedBadMessage(process_id,
                                    bad_message::INVALID_INITIATOR_ORIGIN);
    return false;
  }

  return true;
}

}  // namespace

bool VerifyDownloadUrlParams(RenderProcessHost* process,
                             const blink::mojom::DownloadURLParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(process);
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
                         RenderProcessHost* process,
                         const blink::mojom::OpenURLParamsPtr& params,
                         GURL* out_validated_url,
                         scoped_refptr<network::SharedURLLoaderFactory>*
                             out_blob_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(current_rfh);
  DCHECK(process);
  DCHECK(out_validated_url);
  DCHECK(out_blob_url_loader_factory);
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
            process->GetStoragePartition(), std::move(params->blob_url_token));
  }

  // Verify |params.post_body|.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanReadRequestBody(process, params->post_body)) {
    bad_message::ReceivedBadMessage(process,
                                    bad_message::ILLEGAL_UPLOAD_PARAMS);
    return false;
  }

  // Verify |params.initiator_origin|.
  if (!VerifyInitiatorOrigin(process_id, params->initiator_origin, current_rfh,
                             &params->url, &params->initiator_frame_token)) {
    return false;
  }

  if (params->initiator_base_url) {
    // `initiator_base_url` should only be defined for about:blank and
    // about:srcdoc navigations, and should never be an empty GURL (if it is not
    // nullopt).
    if (params->initiator_base_url->is_empty() ||
        !(out_validated_url->IsAboutBlank() ||
          out_validated_url->IsAboutSrcdoc())) {
      return false;
    }
  }

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
    blink::mojom::CommonNavigationParams* common_params,
    std::optional<blink::LocalFrameToken>& initiator_frame_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(common_params);
  RenderProcessHost* process = current_rfh.GetProcess();
  int process_id = process->GetID();

  // Verify (and possibly rewrite) |url|.
  process->FilterURL(false, &common_params->url);
  if (common_params->url.SchemeIs(kChromeErrorScheme)) {
    mojo::ReportBadMessage("Renderer cannot request error page URLs directly");
    return false;
  }

  // Verify |post_data|.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanReadRequestBody(process, common_params->post_data)) {
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
  if (!VerifyInitiatorOrigin(
          process_id, common_params->initiator_origin.value(), &current_rfh,
          &common_params->url, &initiator_frame_token)) {
    return false;
  }

  // Verify |base_url_for_data_url|.
  if (!common_params->base_url_for_data_url.is_empty()) {
    // Kills the process. http://crbug.com/726142
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_BASE_URL_FOR_DATA_URL_SPECIFIED);
    return false;
  }

  // Verify |initiator_base_url|. The value is allowed to be nullopt, but if it
  // isn't then it's required to be non-empty (the renderer is supposed to
  // guarantee this). If this condition isn't met, CHECK in NavigationRequest's
  // constructor will fail.
  if (common_params->initiator_base_url &&
      common_params->initiator_base_url->is_empty()) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_INITIATOR_BASE_URL_IS_EMPTY);
    return false;
  }

  // Asynchronous (browser-controlled, but) renderer-initiated navigations can
  // not be same-document. Allowing this incorrectly could have us try to
  // navigate an existing document to a different site.
  if (NavigationTypeUtils::IsSameDocument(common_params->navigation_type))
    return false;

  // Verification succeeded.
  return true;
}

bool VerifyNavigationInitiator(
    RenderFrameHostImpl* current_rfh,
    const std::optional<blink::LocalFrameToken>& initiator_frame_token,
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
  std::optional<base::UnguessableToken> initiator_fenced_frame_nonce =
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
