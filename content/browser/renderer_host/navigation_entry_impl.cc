// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_entry_impl.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/web_package/web_bundle_navigation_info.h"
#include "content/common/content_constants_internal.h"
#include "content/common/navigation_params.h"
#include "content/common/page_state_serialization.h"
#include "content/public/browser/reload_type.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "ui/gfx/text_elider.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

using base::UTF16ToUTF8;

namespace content {

namespace {

// Use this to get a new unique ID for a NavigationEntry during construction.
// The returned ID is guaranteed to be nonzero (which is the "no ID" indicator).
int CreateUniqueEntryID() {
  static int unique_id_counter = 0;
  return ++unique_id_counter;
}

void RecursivelyGenerateFrameEntries(
    const ExplodedFrameState& state,
    const std::vector<base::Optional<base::string16>>& referenced_files,
    NavigationEntryImpl::TreeNode* node) {
  // Set a single-frame PageState on the entry.
  ExplodedPageState page_state;

  // Copy the incoming PageState's list of referenced files into the main
  // frame's PageState.  (We don't pass the list to subframes below.)
  // TODO(creis): Grant access to this list for each process that renders
  // this page, even for OOPIFs.  Eventually keep track of a verified list of
  // files per frame, so that we only grant access to processes that need it.
  if (referenced_files.size() > 0)
    page_state.referenced_files = referenced_files;

  page_state.top = state;
  std::string data;
  EncodePageState(page_state, &data);
  DCHECK(!data.empty()) << "Shouldn't generate an empty PageState.";

  node->frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
      UTF16ToUTF8(state.target.value_or(base::string16())),
      state.item_sequence_number, state.document_sequence_number, nullptr,
      nullptr, GURL(state.url_string.value_or(base::string16())),
      // TODO(nasko): Supply valid origin once the value is persisted across
      // session restore.
      nullptr /* origin */,
      Referrer(GURL(state.referrer.value_or(base::string16())),
               state.referrer_policy),
      state.initiator_origin, std::vector<GURL>(),
      PageState::CreateFromEncodedData(data), "GET", -1,
      nullptr /* blob_url_loader_factory */,
      nullptr /* web_bundle_navigation_info */);

  // Don't pass the file list to subframes, since that would result in multiple
  // copies of it ending up in the combined list in GetPageState (via
  // RecursivelyGenerateFrameState).
  std::vector<base::Optional<base::string16>> empty_file_list;

  for (const ExplodedFrameState& child_state : state.children) {
    node->children.push_back(
        std::make_unique<NavigationEntryImpl::TreeNode>(node, nullptr));
    RecursivelyGenerateFrameEntries(child_state, empty_file_list,
                                    node->children.back().get());
  }
}

base::Optional<base::string16> UrlToOptionalString16(const GURL& url) {
  if (!url.is_valid())
    return base::nullopt;
  return base::UTF8ToUTF16(url.spec());
}

void RecursivelyGenerateFrameState(
    NavigationEntryImpl::TreeNode* node,
    ExplodedFrameState* state,
    std::vector<base::Optional<base::string16>>* referenced_files) {
  // The FrameNavigationEntry's PageState contains just the ExplodedFrameState
  // for that particular frame.
  ExplodedPageState exploded_page_state;
  if (!DecodePageState(node->frame_entry->page_state().ToEncodedData(),
                       &exploded_page_state)) {
    NOTREACHED();
    return;
  }
  ExplodedFrameState frame_state = exploded_page_state.top;

  // Copy the FrameNavigationEntry's frame state into the destination state.
  *state = frame_state;

  // Some data is stored *both* in 1) PageState/ExplodedFrameState and 2)
  // FrameNavigationEntry.  We want to treat FrameNavigationEntry as the
  // authoritative source of the data, so we clobber the ExplodedFrameState with
  // the data taken from FrameNavigationEntry.
  //
  // The following ExplodedFrameState fields do not have an equivalent
  // FrameNavigationEntry field:
  // - target
  // - state_object
  // - document_state
  // - scroll_restoration_type
  // - did_save_scroll_or_scale_state
  // - visual_viewport_scroll_offset
  // - scroll_offset
  // - page_scale_factor
  // - http_body (FrameNavigationEntry::GetPostData extracts the body from
  //   the ExplodedFrameState)
  // - scroll_anchor_selector
  // - scroll_anchor_offset
  // - scroll_anchor_simhash
  state->url_string = UrlToOptionalString16(node->frame_entry->url());
  state->referrer = UrlToOptionalString16(node->frame_entry->referrer().url);
  state->referrer_policy = node->frame_entry->referrer().policy;
  state->item_sequence_number = node->frame_entry->item_sequence_number();
  state->document_sequence_number =
      node->frame_entry->document_sequence_number();
  state->initiator_origin = node->frame_entry->initiator_origin();

  // Copy the frame's files into the PageState's |referenced_files|.
  referenced_files->reserve(referenced_files->size() +
                            exploded_page_state.referenced_files.size());
  for (auto& file : exploded_page_state.referenced_files)
    referenced_files->emplace_back(file);

  state->children.resize(node->children.size());
  for (size_t i = 0; i < node->children.size(); ++i) {
    RecursivelyGenerateFrameState(node->children[i].get(), &state->children[i],
                                  referenced_files);
  }
}

// Walk the ancestor chain for both the |frame_tree_node| and the |node|.
// Comparing the inputs directly is not performed, as this method assumes they
// already match each other. Returns false if a mismatch in unique name or
// ancestor chain is detected, otherwise true.
bool InSameTreePosition(FrameTreeNode* frame_tree_node,
                        NavigationEntryImpl::TreeNode* node) {
  FrameTreeNode* ftn = FrameTreeNode::From(frame_tree_node->parent());
  NavigationEntryImpl::TreeNode* current_node = node->parent;
  while (ftn && current_node) {
    if (!current_node->MatchesFrame(ftn))
      return false;

    if ((!current_node->parent && ftn->parent()) ||
        (current_node->parent && !ftn->parent())) {
      return false;
    }

    ftn = FrameTreeNode::From(ftn->parent());
    current_node = current_node->parent;
  }
  return true;
}

void RegisterOriginsRecursive(NavigationEntryImpl::TreeNode* node,
                              const url::Origin& origin) {
  if (node->frame_entry->committed_origin().has_value()) {
    const url::Origin node_origin =
        node->frame_entry->committed_origin().value();
    SiteInstanceImpl* site_instance = node->frame_entry->site_instance();
    if (site_instance && origin == node_origin)
      site_instance->PreventOptInOriginIsolation(node_origin);
  }

  for (auto& child : node->children)
    RegisterOriginsRecursive(child.get(), origin);
}

}  // namespace

void NavigationEntryImpl::RegisterExistingOriginToPreventOptInIsolation(
    const url::Origin& origin) {
  return RegisterOriginsRecursive(root_node(), origin);
}

NavigationEntryImpl::TreeNode::TreeNode(
    TreeNode* parent,
    scoped_refptr<FrameNavigationEntry> frame_entry)
    : parent(parent), frame_entry(std::move(frame_entry)) {}

NavigationEntryImpl::TreeNode::~TreeNode() {}

bool NavigationEntryImpl::TreeNode::MatchesFrame(
    FrameTreeNode* frame_tree_node) const {
  // The root node is for the main frame whether the unique name matches or not.
  if (!parent)
    return frame_tree_node->IsMainFrame();

  // Otherwise check the unique name for subframes.
  return !frame_tree_node->IsMainFrame() &&
         frame_tree_node->unique_name() == frame_entry->frame_unique_name();
}

std::unique_ptr<NavigationEntryImpl::TreeNode>
NavigationEntryImpl::TreeNode::CloneAndReplace(
    scoped_refptr<FrameNavigationEntry> frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* current_frame_tree_node,
    TreeNode* parent_node) const {
  // Clone this TreeNode, possibly replacing its FrameNavigationEntry.
  bool is_target_frame =
      target_frame_tree_node && MatchesFrame(target_frame_tree_node);
  auto copy = std::make_unique<NavigationEntryImpl::TreeNode>(
      parent_node,
      is_target_frame ? frame_navigation_entry : frame_entry->Clone());

  // Recursively clone the children if needed.
  if (!is_target_frame || clone_children_of_target) {
    for (size_t i = 0; i < children.size(); i++) {
      const auto& child = children[i];

      // Don't check whether it's still in the tree if |current_frame_tree_node|
      // is null.
      if (!current_frame_tree_node) {
        copy->children.push_back(child->CloneAndReplace(
            frame_navigation_entry, clone_children_of_target,
            target_frame_tree_node, nullptr, copy.get()));
        continue;
      }

      // Otherwise, make sure the frame is still in the tree before cloning it.
      // This is O(N^2) in the worst case because we need to look up each frame
      // (and since we want to avoid a map of unique names, which can be very
      // long).  To partly mitigate this, we add an optimization for the common
      // case that the two child lists are the same length and are likely in the
      // same order: we pick the starting offset of the inner loop to get O(N).
      size_t ftn_child_count = current_frame_tree_node->child_count();
      for (size_t j = 0; j < ftn_child_count; j++) {
        size_t index = j;
        // If the two lists of children are the same length, start looking at
        // the same index as |child|.
        if (children.size() == ftn_child_count)
          index = (i + j) % ftn_child_count;

        if (current_frame_tree_node->child_at(index)->unique_name() ==
            child->frame_entry->frame_unique_name()) {
          // Found |child| in the tree.  Clone it and break out of inner loop.
          copy->children.push_back(child->CloneAndReplace(
              frame_navigation_entry, clone_children_of_target,
              target_frame_tree_node, current_frame_tree_node->child_at(index),
              copy.get()));
          break;
        }
      }
    }
  }

  return copy;
}

std::unique_ptr<NavigationEntry> NavigationEntry::Create() {
  return std::make_unique<NavigationEntryImpl>();
}

NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    NavigationEntry* entry) {
  return static_cast<NavigationEntryImpl*>(entry);
}

const NavigationEntryImpl* NavigationEntryImpl::FromNavigationEntry(
    const NavigationEntry* entry) {
  return static_cast<const NavigationEntryImpl*>(entry);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::FromNavigationEntry(
    std::unique_ptr<NavigationEntry> entry) {
  return base::WrapUnique(FromNavigationEntry(entry.release()));
}

NavigationEntryImpl::NavigationEntryImpl()
    : NavigationEntryImpl(nullptr,
                          GURL(),
                          Referrer(),
                          base::nullopt,
                          base::string16(),
                          ui::PAGE_TRANSITION_LINK,
                          false,
                          nullptr) {}

NavigationEntryImpl::NavigationEntryImpl(
    scoped_refptr<SiteInstanceImpl> instance,
    const GURL& url,
    const Referrer& referrer,
    const base::Optional<url::Origin>& initiator_origin,
    const base::string16& title,
    ui::PageTransition transition_type,
    bool is_renderer_initiated,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory)
    : frame_tree_(std::make_unique<TreeNode>(
          nullptr,
          base::MakeRefCounted<FrameNavigationEntry>(
              "",
              -1,
              -1,
              std::move(instance),
              nullptr,
              url,
              nullptr /* origin */,
              referrer,
              initiator_origin,
              std::vector<GURL>(),
              PageState(),
              "GET",
              -1,
              std::move(blob_url_loader_factory),
              nullptr /* web_bundle_navigation_info */))),
      unique_id_(CreateUniqueEntryID()),
      page_type_(PAGE_TYPE_NORMAL),
      update_virtual_url_with_url_(false),
      title_(title),
      transition_type_(transition_type),
      restore_type_(RestoreType::NONE),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_renderer_initiated_(is_renderer_initiated),
      should_replace_entry_(false),
      should_clear_history_list_(false),
      can_load_local_resources_(false),
      frame_tree_node_id_(-1),
      has_user_gesture_(false),
      reload_type_(ReloadType::NONE),
      started_from_context_menu_(false),
      ssl_error_(false),
      should_skip_on_back_forward_ui_(false) {}

NavigationEntryImpl::~NavigationEntryImpl() {}

int NavigationEntryImpl::GetUniqueID() {
  return unique_id_;
}

PageType NavigationEntryImpl::GetPageType() {
  return page_type_;
}

void NavigationEntryImpl::SetURL(const GURL& url) {
  frame_tree_->frame_entry->set_url(url);
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetURL() {
  return frame_tree_->frame_entry->url();
}

void NavigationEntryImpl::SetBaseURLForDataURL(const GURL& url) {
  base_url_for_data_url_ = url;
}

const GURL& NavigationEntryImpl::GetBaseURLForDataURL() {
  return base_url_for_data_url_;
}

#if defined(OS_ANDROID)
void NavigationEntryImpl::SetDataURLAsString(
    scoped_refptr<base::RefCountedString> data_url) {
  if (data_url) {
    // A quick check that it's actually a data URL.
    DCHECK(base::StartsWith(data_url->front_as<char>(), url::kDataScheme,
                            base::CompareCase::SENSITIVE));
  }
  data_url_as_string_ = std::move(data_url);
}

const scoped_refptr<const base::RefCountedString>&
NavigationEntryImpl::GetDataURLAsString() {
  return data_url_as_string_;
}
#endif

void NavigationEntryImpl::SetReferrer(const Referrer& referrer) {
  frame_tree_->frame_entry->set_referrer(referrer);
}

const Referrer& NavigationEntryImpl::GetReferrer() {
  return frame_tree_->frame_entry->referrer();
}

void NavigationEntryImpl::SetVirtualURL(const GURL& url) {
  virtual_url_ = (url == GetURL()) ? GURL() : url;
  cached_display_title_.clear();
}

const GURL& NavigationEntryImpl::GetVirtualURL() {
  return virtual_url_.is_empty() ? GetURL() : virtual_url_;
}

void NavigationEntryImpl::SetTitle(const base::string16& title) {
  title_ = title;
  cached_display_title_.clear();
}

const base::string16& NavigationEntryImpl::GetTitle() {
  return title_;
}

void NavigationEntryImpl::SetPageState(const PageState& state) {
  DCHECK(state.IsValid());

  // SetPageState should only be called before the NavigationEntry has been
  // loaded, such as for restore (when there are no subframe
  // FrameNavigationEntries yet).  However, some callers expect to call this
  // after a Clone but before loading the page.  Clone will copy over the
  // subframe entries, and we should reset them before setting the state again.
  //
  // TODO(creis): It would be good to verify that this NavigationEntry hasn't
  // been loaded yet in cases that SetPageState is called while subframe
  // entries exist, but there's currently no way to check that.
  if (!frame_tree_->children.empty())
    frame_tree_->children.clear();

  // If the PageState can't be parsed, just store it on the main frame's
  // FrameNavigationEntry without recursively creating subframe entries.
  ExplodedPageState exploded_state;
  if (!DecodePageState(state.ToEncodedData(), &exploded_state)) {
    frame_tree_->frame_entry->SetPageState(state);
    return;
  }

  RecursivelyGenerateFrameEntries(
      exploded_state.top, exploded_state.referenced_files, frame_tree_.get());
}

PageState NavigationEntryImpl::GetPageState() {
  // Each FrameNavigationEntry has a frame-specific PageState.  We combine these
  // into an ExplodedPageState tree and generate a full PageState from it.
  ExplodedPageState exploded_state;
  RecursivelyGenerateFrameState(frame_tree_.get(), &exploded_state.top,
                                &exploded_state.referenced_files);

  std::string encoded_data;
  EncodePageState(exploded_state, &encoded_data);
  return PageState::CreateFromEncodedData(encoded_data);
}

void NavigationEntryImpl::set_site_instance(
    scoped_refptr<SiteInstanceImpl> site_instance) {
  // TODO(creis): Update all callers and remove this method.
  frame_tree_->frame_entry->set_site_instance(std::move(site_instance));
}

const base::string16& NavigationEntryImpl::GetTitleForDisplay() {
  // Most pages have real titles. Don't even bother caching anything if this is
  // the case.
  if (!title_.empty())
    return title_;

  // More complicated cases will use the URLs as the title. This result we will
  // cache since it's more complicated to compute.
  if (!cached_display_title_.empty())
    return cached_display_title_;

  // Use the virtual URL first if any, and fall back on using the real URL.
  base::string16 title;
  if (!virtual_url_.is_empty()) {
    title = url_formatter::FormatUrl(virtual_url_);
  } else if (!GetURL().is_empty()) {
    title = url_formatter::FormatUrl(GetURL());
  }

  // For file:// URLs use the filename as the title, not the full path.
  if (GetURL().SchemeIsFile()) {
    // It is necessary to ignore the reference and query parameters or else
    // looking for slashes might accidentally return one of those values. See
    // https://crbug.com/503003.
    base::string16::size_type refpos = title.find('#');
    base::string16::size_type querypos = title.find('?');
    base::string16::size_type lastpos;
    if (refpos == base::string16::npos)
      lastpos = querypos;
    else if (querypos == base::string16::npos)
      lastpos = refpos;
    else
      lastpos = (refpos < querypos) ? refpos : querypos;
    base::string16::size_type slashpos = title.rfind('/', lastpos);
    if (slashpos != base::string16::npos)
      title = title.substr(slashpos + 1);

  } else if (GetURL().SchemeIs(kChromeUIUntrustedScheme)) {
    // For chrome-untrusted:// URLs, leave title blank until the page loads.
    title = base::string16();

  } else if (base::i18n::StringContainsStrongRTLChars(title)) {
    // Wrap the URL in an LTR embedding for proper handling of RTL characters.
    // (RFC 3987 Section 4.1 states that "Bidirectional IRIs MUST be rendered in
    // the same way as they would be if they were in a left-to-right
    // embedding".)
    base::i18n::WrapStringWithLTRFormatting(&title);
  }

#if defined(OS_ANDROID)
  if (GetURL().SchemeIs(url::kContentScheme)) {
    base::string16 file_display_name;
    if (base::MaybeGetFileDisplayName(base::FilePath(GetURL().spec()),
                                      &file_display_name)) {
      title = file_display_name;
    }
  }
#endif

  gfx::ElideString(title, blink::mojom::kMaxTitleChars, &cached_display_title_);
  return cached_display_title_;
}

bool NavigationEntryImpl::IsViewSourceMode() {
  return virtual_url_.SchemeIs(kViewSourceScheme);
}

void NavigationEntryImpl::SetTransitionType(
    ui::PageTransition transition_type) {
  transition_type_ = transition_type;
}

ui::PageTransition NavigationEntryImpl::GetTransitionType() {
  return transition_type_;
}

const GURL& NavigationEntryImpl::GetUserTypedURL() {
  return user_typed_url_;
}

void NavigationEntryImpl::SetHasPostData(bool has_post_data) {
  frame_tree_->frame_entry->set_method(has_post_data ? "POST" : "GET");
}

bool NavigationEntryImpl::GetHasPostData() {
  return frame_tree_->frame_entry->method() == "POST";
}

void NavigationEntryImpl::SetPostID(int64_t post_id) {
  frame_tree_->frame_entry->set_post_id(post_id);
}

int64_t NavigationEntryImpl::GetPostID() {
  return frame_tree_->frame_entry->post_id();
}

void NavigationEntryImpl::SetPostData(
    const scoped_refptr<network::ResourceRequestBody>& data) {
  post_data_ = static_cast<network::ResourceRequestBody*>(data.get());
}

scoped_refptr<network::ResourceRequestBody> NavigationEntryImpl::GetPostData() {
  return post_data_.get();
}

FaviconStatus& NavigationEntryImpl::GetFavicon() {
  return favicon_;
}

SSLStatus& NavigationEntryImpl::GetSSL() {
  return ssl_;
}

void NavigationEntryImpl::SetOriginalRequestURL(const GURL& original_url) {
  original_request_url_ = original_url;
}

const GURL& NavigationEntryImpl::GetOriginalRequestURL() {
  return original_request_url_;
}

void NavigationEntryImpl::SetIsOverridingUserAgent(bool override_ua) {
  is_overriding_user_agent_ = override_ua;
}

bool NavigationEntryImpl::GetIsOverridingUserAgent() {
  return is_overriding_user_agent_;
}

void NavigationEntryImpl::SetTimestamp(base::Time timestamp) {
  timestamp_ = timestamp;
}

base::Time NavigationEntryImpl::GetTimestamp() {
  return timestamp_;
}

void NavigationEntryImpl::SetHttpStatusCode(int http_status_code) {
  http_status_code_ = http_status_code;
}

int NavigationEntryImpl::GetHttpStatusCode() {
  return http_status_code_;
}

void NavigationEntryImpl::SetRedirectChain(
    const std::vector<GURL>& redirect_chain) {
  root_node()->frame_entry->set_redirect_chain(redirect_chain);
}

const std::vector<GURL>& NavigationEntryImpl::GetRedirectChain() {
  return root_node()->frame_entry->redirect_chain();
}

const base::Optional<ReplacedNavigationEntryData>&
NavigationEntryImpl::GetReplacedEntryData() {
  return replaced_entry_data_;
}

bool NavigationEntryImpl::IsRestored() {
  return restore_type_ != RestoreType::NONE;
}

std::string NavigationEntryImpl::GetExtraHeaders() {
  return extra_headers_;
}

void NavigationEntryImpl::AddExtraHeaders(
    const std::string& more_extra_headers) {
  DCHECK(!more_extra_headers.empty());
  if (!extra_headers_.empty())
    extra_headers_ += "\r\n";
  extra_headers_ += more_extra_headers;
}

int64_t NavigationEntryImpl::GetMainFrameDocumentSequenceNumber() {
  return frame_tree_->frame_entry->document_sequence_number();
}

void NavigationEntryImpl::SetCanLoadLocalResources(bool allow) {
  can_load_local_resources_ = allow;
}

bool NavigationEntryImpl::GetCanLoadLocalResources() {
  return can_load_local_resources_;
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::Clone() const {
  return NavigationEntryImpl::CloneAndReplace(nullptr, false, nullptr, nullptr);
}

std::unique_ptr<NavigationEntryImpl> NavigationEntryImpl::CloneAndReplace(
    scoped_refptr<FrameNavigationEntry> frame_navigation_entry,
    bool clone_children_of_target,
    FrameTreeNode* target_frame_tree_node,
    FrameTreeNode* root_frame_tree_node) const {
  auto copy = std::make_unique<NavigationEntryImpl>();

  // TODO(creis): Only share the same FrameNavigationEntries if cloning within
  // the same tab.
  copy->frame_tree_ = frame_tree_->CloneAndReplace(
      std::move(frame_navigation_entry), clone_children_of_target,
      target_frame_tree_node, root_frame_tree_node, nullptr);

  // Copy most state over, unless cleared in ResetForCommit.
  // Don't copy unique_id_, otherwise it won't be unique.
  copy->page_type_ = page_type_;
  copy->virtual_url_ = virtual_url_;
  copy->update_virtual_url_with_url_ = update_virtual_url_with_url_;
  copy->title_ = title_;
  copy->favicon_ = favicon_;
  copy->ssl_ = ssl_;
  copy->transition_type_ = transition_type_;
  copy->user_typed_url_ = user_typed_url_;
  copy->restore_type_ = restore_type_;
  copy->original_request_url_ = original_request_url_;
  copy->is_overriding_user_agent_ = is_overriding_user_agent_;
  copy->timestamp_ = timestamp_;
  copy->http_status_code_ = http_status_code_;
  // ResetForCommit: post_data_
  copy->extra_headers_ = extra_headers_;
  copy->base_url_for_data_url_ = base_url_for_data_url_;
#if defined(OS_ANDROID)
  copy->data_url_as_string_ = data_url_as_string_;
#endif
  // ResetForCommit: is_renderer_initiated_
  copy->cached_display_title_ = cached_display_title_;
  // ResetForCommit: should_replace_entry_
  // ResetForCommit: should_clear_history_list_
  // ResetForCommit: frame_tree_node_id_
  copy->has_user_gesture_ = has_user_gesture_;
  // ResetForCommit: reload_type_
  copy->CloneDataFrom(*this);
  copy->replaced_entry_data_ = replaced_entry_data_;
  copy->should_skip_on_back_forward_ui_ = should_skip_on_back_forward_ui_;

  return copy;
}

mojom::CommonNavigationParamsPtr
NavigationEntryImpl::ConstructCommonNavigationParams(
    const FrameNavigationEntry& frame_entry,
    const scoped_refptr<network::ResourceRequestBody>& post_body,
    const GURL& dest_url,
    blink::mojom::ReferrerPtr dest_referrer,
    mojom::NavigationType navigation_type,
    blink::PreviewsState previews_state,
    base::TimeTicks navigation_start,
    base::TimeTicks input_start) {
  NavigationDownloadPolicy download_policy;
  if (IsViewSourceMode())
    download_policy.SetDisallowed(NavigationDownloadType::kViewSource);

  return mojom::CommonNavigationParams::New(
      dest_url, frame_entry.initiator_origin(), std::move(dest_referrer),
      GetTransitionType(), navigation_type, download_policy,
      should_replace_entry(), GetBaseURLForDataURL(), GetHistoryURLForDataURL(),
      previews_state, navigation_start, frame_entry.method(),
      post_body ? post_body : post_data_, network::mojom::SourceLocation::New(),
      has_started_from_context_menu(), has_user_gesture(),
      false /* has_text_fragment_token */, CreateInitiatorCSPInfo(),
      std::vector<int>(), std::string(),
      false /* is_history_navigation_in_new_child_frame */, input_start);
}

mojom::CommitNavigationParamsPtr
NavigationEntryImpl::ConstructCommitNavigationParams(
    const FrameNavigationEntry& frame_entry,
    const GURL& original_url,
    const base::Optional<url::Origin>& origin_to_commit,
    const std::string& original_method,
    const base::flat_map<std::string, bool>& subframe_unique_names,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length,
    const blink::FramePolicy& frame_policy) {
  // Set the redirect chain to the navigation's redirects, unless returning to a
  // completed navigation (whose previous redirects don't apply).
  std::vector<GURL> redirects;
  if (ui::PageTransitionIsNewNavigation(GetTransitionType())) {
    redirects = frame_entry.redirect_chain();
  }

  int pending_offset_to_send = pending_history_list_offset;
  int current_offset_to_send = current_history_list_offset;
  int current_length_to_send = current_history_list_length;
  if (should_clear_history_list()) {
    // Set the history list related parameters to the same values a
    // NavigationController would return before its first navigation. This will
    // fully clear the RenderView's view of the session history.
    pending_offset_to_send = -1;
    current_offset_to_send = -1;
    current_length_to_send = 0;
  }

  mojom::CommitNavigationParamsPtr commit_params =
      mojom::CommitNavigationParams::New(
          origin_to_commit, GetIsOverridingUserAgent(), redirects,
          std::vector<network::mojom::URLResponseHeadPtr>(),
          std::vector<net::RedirectInfo>(), std::string(), original_url,
          original_method, GetCanLoadLocalResources(), frame_entry.page_state(),
          GetUniqueID(), subframe_unique_names, intended_as_new_entry,
          pending_offset_to_send, current_offset_to_send,
          current_length_to_send, false, IsViewSourceMode(),
          should_clear_history_list(), mojom::NavigationTiming::New(),
          base::nullopt, mojom::WasActivatedOption::kUnknown,
          base::UnguessableToken::Create(),
          std::vector<mojom::PrefetchedSignedExchangeInfoPtr>(),
#if defined(OS_ANDROID)
          std::string(),
#endif
          false, network::mojom::IPAddressSpace::kUnknown,
          GURL() /* web_bundle_physical_url */,
          GURL() /* base_url_override_for_web_bundle */,
          ukm::kInvalidSourceId /* document_ukm_source_id */, frame_policy,
          std::vector<std::string>() /* force_enabled_origin_trials */,
          false /* origin_isolated */,
          std::vector<
              network::mojom::WebClientHintsType>() /* enabled_client_hints */,
          false /* is_cross_browsing_instance */,
          std::vector<std::string>() /* forced_content_security_policies */,
          nullptr /* old_page_info */);
#if defined(OS_ANDROID)
  if (NavigationControllerImpl::ValidateDataURLAsString(GetDataURLAsString())) {
    commit_params->data_url_as_string = GetDataURLAsString()->data();
  }
#endif
  return commit_params;
}

void NavigationEntryImpl::ResetForCommit(FrameNavigationEntry* frame_entry) {
  // Any state that only matters when a navigation entry is pending should be
  // cleared here.
  // TODO(creis): This state should be moved to NavigationRequest.
  SetPostData(nullptr);
  set_is_renderer_initiated(false);
  set_should_replace_entry(false);

  set_should_clear_history_list(false);
  set_frame_tree_node_id(-1);
  set_reload_type(ReloadType::NONE);

  if (frame_entry) {
    frame_entry->set_source_site_instance(nullptr);
    frame_entry->set_blob_url_loader_factory(nullptr);
  }
}

NavigationEntryImpl::TreeNode* NavigationEntryImpl::GetTreeNode(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* node = nullptr;
  base::queue<NavigationEntryImpl::TreeNode*> work_queue;
  work_queue.push(root_node());
  while (!work_queue.empty()) {
    node = work_queue.front();
    work_queue.pop();
    if (node->MatchesFrame(frame_tree_node))
      return node;

    // Enqueue any children and keep looking.
    for (const auto& child : node->children)
      work_queue.push(child.get());
  }
  return nullptr;
}

void NavigationEntryImpl::AddOrUpdateFrameEntry(
    FrameTreeNode* frame_tree_node,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    SiteInstanceImpl* site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const base::Optional<url::Origin>& origin,
    const Referrer& referrer,
    const base::Optional<url::Origin>& initiator_origin,
    const std::vector<GURL>& redirect_chain,
    const PageState& page_state,
    const std::string& method,
    int64_t post_id,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    std::unique_ptr<WebBundleNavigationInfo> web_bundle_navigation_info) {
  // If this is called for the main frame, the FrameNavigationEntry is
  // guaranteed to exist, so just update it directly and return.
  if (frame_tree_node->IsMainFrame()) {
    // If the document of the FrameNavigationEntry is changing, we must clear
    // any child FrameNavigationEntries.
    if (root_node()->frame_entry->document_sequence_number() !=
        document_sequence_number)
      root_node()->children.clear();

    root_node()->frame_entry->UpdateEntry(
        frame_tree_node->unique_name(), item_sequence_number,
        document_sequence_number, site_instance,
        std::move(source_site_instance), url, origin, referrer,
        initiator_origin, redirect_chain, page_state, method, post_id,
        std::move(blob_url_loader_factory),
        std::move(web_bundle_navigation_info));
    return;
  }

  // We should already have a TreeNode for the parent node by the time this node
  // commits.  Find it first.
  NavigationEntryImpl::TreeNode* parent_node =
      GetTreeNode(FrameTreeNode::From(frame_tree_node->parent()));
  if (!parent_node) {
    // The renderer should not send a commit for a subframe before its parent.
    // TODO(creis): Kill the renderer if we get here.
    return;
  }

  // Now check whether we have a TreeNode for the node itself.
  const std::string& unique_name = frame_tree_node->unique_name();
  for (const auto& child : parent_node->children) {
    if (child->frame_entry->frame_unique_name() == unique_name) {
      // If the document of the FrameNavigationEntry is changing, we must clear
      // any child FrameNavigationEntries.
      if (child->frame_entry->document_sequence_number() !=
          document_sequence_number)
        child->children.clear();

      // Update the existing FrameNavigationEntry (e.g., for replaceState).
      child->frame_entry->UpdateEntry(
          unique_name, item_sequence_number, document_sequence_number,
          site_instance, std::move(source_site_instance), url, origin, referrer,
          initiator_origin, redirect_chain, page_state, method, post_id,
          std::move(blob_url_loader_factory),
          std::move(web_bundle_navigation_info));
      return;
    }
  }

  // No entry exists yet, so create a new one.
  // Unordered list, since we expect to look up entries by frame sequence number
  // or unique name.
  auto frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
      unique_name, item_sequence_number, document_sequence_number,
      site_instance, std::move(source_site_instance), url,
      base::OptionalOrNullptr(origin), referrer, initiator_origin,
      redirect_chain, page_state, method, post_id,
      std::move(blob_url_loader_factory),
      std::move(web_bundle_navigation_info));
  parent_node->children.push_back(
      std::make_unique<NavigationEntryImpl::TreeNode>(parent_node,
                                                      std::move(frame_entry)));
}

FrameNavigationEntry* NavigationEntryImpl::GetFrameEntry(
    FrameTreeNode* frame_tree_node) const {
  NavigationEntryImpl::TreeNode* tree_node = GetTreeNode(frame_tree_node);
  return tree_node ? tree_node->frame_entry.get() : nullptr;
}

base::flat_map<std::string, bool> NavigationEntryImpl::GetSubframeUniqueNames(
    FrameTreeNode* frame_tree_node) const {
  base::flat_map<std::string, bool> names;
  NavigationEntryImpl::TreeNode* tree_node = GetTreeNode(frame_tree_node);
  if (tree_node) {
    // Return the names of all immediate children.
    for (const auto& child : tree_node->children) {
      // Keep track of whether we would be loading about:blank, since the
      // renderer should be allowed to just commit the initial blank frame if
      // that was the default URL.  PageState doesn't matter there, because
      // content injected into about:blank frames doesn't use it.
      //
      // Be careful not to rely on FrameNavigationEntry's URLs in this check,
      // because the committed URL in the browser could be rewritten to
      // about:blank.
      // See RenderProcessHostImpl::FilterURL to know which URLs are rewritten.
      // See https://crbug.com/657896 for details.
      bool is_about_blank = false;
      ExplodedPageState exploded_page_state;
      if (DecodePageState(child->frame_entry->page_state().ToEncodedData(),
                          &exploded_page_state)) {
        ExplodedFrameState frame_state = exploded_page_state.top;
        if (UTF16ToUTF8(frame_state.url_string.value_or(base::string16())) ==
            url::kAboutBlankURL)
          is_about_blank = true;
      }

      names[child->frame_entry->frame_unique_name()] = is_about_blank;
    }
  }
  return names;
}

void NavigationEntryImpl::RemoveEntryForFrame(FrameTreeNode* frame_tree_node,
                                              bool only_if_different_position) {
  DCHECK(!frame_tree_node->IsMainFrame());

  NavigationEntryImpl::TreeNode* node = GetTreeNode(frame_tree_node);
  if (!node)
    return;

  // Remove the |node| from the tree if either 1) |only_if_different_position|
  // was not asked for or 2) if it is not in the same position in the tree of
  // FrameNavigationEntries and the FrameTree.
  if (!only_if_different_position ||
      !InSameTreePosition(frame_tree_node, node)) {
    auto* frame_entry = node->frame_entry.get();
    if (frame_entry && frame_entry->committed_origin()) {
      // Normally non-isolated origins are tracked through their presence in
      // session history, which is consulted whenever an origin newly requests
      // isolation. If we remove a frame_entry, its origin won't be available
      // to any future global walk if the same origin later wants to opt-in. So
      // we add it to the non-opt-in list here to be spec compliant (unless it's
      // currently opted-in, in which case this call will do nothing).
      ChildProcessSecurityPolicyImpl::GetInstance()
          ->AddNonIsolatedOriginIfNeeded(
              frame_entry->site_instance()->GetIsolationContext(),
              frame_entry->committed_origin().value(),
              true /* global_ walk_or_frame_removal */);
    }
    NavigationEntryImpl::TreeNode* parent_node = node->parent;
    auto it = std::find_if(
        parent_node->children.begin(), parent_node->children.end(),
        [node](const std::unique_ptr<NavigationEntryImpl::TreeNode>& item) {
          return item.get() == node;
        });
    CHECK(it != parent_node->children.end());
    parent_node->children.erase(it);
  }
}

GURL NavigationEntryImpl::GetHistoryURLForDataURL() {
  return GetBaseURLForDataURL().is_empty() ? GURL() : GetVirtualURL();
}

}  // namespace content
