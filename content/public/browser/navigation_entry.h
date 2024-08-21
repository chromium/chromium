// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_ENTRY_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_ENTRY_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/page_type.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace blink {
class PageState;
}

namespace network {
class ResourceRequestBody;
}

namespace content {

struct FaviconStatus;
class NavigationEntryRestoreContext;
struct ReplacedNavigationEntryData;
struct SSLStatus;

// A NavigationEntry is a data structure that captures all the information
// required to recreate a browsing state. This includes some opaque binary
// state as provided by the WebContents as well as some clear text title and
// URL which is used for our user interface.
class NavigationEntry : public base::SupportsUserData {
 public:
  ~NavigationEntry() override {}

  CONTENT_EXPORT static std::unique_ptr<NavigationEntry> Create();

  // True if this entry is the initial NavigationEntry, which is created when a
  // FrameTree is first initialized. The initial NavigationEntry, unlike other
  // NavigationEntries, is not associated with any committed navigation in the
  // main frame. After any navigation committed in the main frame, the
  // NavigationEntry will be replaced, or at least lose its "initial" status.
  virtual bool IsInitialEntry() = 0;

  // Page-related stuff --------------------------------------------------------

  // A unique ID is preserved across commits and redirects, which means that
  // sometimes a NavigationEntry's unique ID needs to be set (e.g. when
  // creating a committed entry to correspond to a to-be-deleted pending entry,
  // the pending entry's ID must be copied).
  virtual int GetUniqueID() = 0;

  // The page type tells us if this entry is for an interstitial or error page.
  virtual content::PageType GetPageType() = 0;

  // The actual URL of the page. For some about pages, this may be a scary
  // data: URL or something like that. Use GetVirtualURL() below for showing to
  // the user.
  virtual void SetURL(const GURL& url) = 0;
  virtual const GURL& GetURL() = 0;

  // Used for specifying a base URL for pages loaded via data URLs.
  virtual void SetBaseURLForDataURL(const GURL& url) = 0;
  virtual const GURL& GetBaseURLForDataURL() = 0;

#if BUILDFLAG(IS_ANDROID)
  // The real data: URL when it is received via WebView.loadDataWithBaseUrl
  // method. Represented as a string to circumvent the size restriction
  // of GURLs for compatibility with legacy Android WebView apps.
  virtual void SetDataURLAsString(
      scoped_refptr<base::RefCountedString> data_url) = 0;
  virtual const scoped_refptr<const base::RefCountedString>&
  GetDataURLAsString() = 0;
#endif

  // The referring URL. Can be empty.
  virtual void SetReferrer(const content::Referrer& referrer) = 0;
  virtual const content::Referrer& GetReferrer() = 0;

  // The virtual URL, when nonempty, will override the actual URL of the page
  // when we display it to the user. This allows us to have nice and friendly
  // URLs that the user sees for things like about: URLs, but actually feed
  // the renderer a data URL that results in the content loading.
  //
  // GetVirtualURL() will return the URL to display to the user in all cases, so
  // if there is no overridden display URL, it will return the actual one.
  virtual void SetVirtualURL(const GURL& url) = 0;
  virtual const GURL& GetVirtualURL() = 0;

  // The title as set by the page. This will be empty if there is no title set.
  // The caller is responsible for detecting when there is no title and
  // displaying the appropriate "Untitled" label if this is being displayed to
  // the user.
  // Use WebContents::UpdateTitleForEntry() in most cases, since that notifies
  // observers when the visible title changes. Only call
  // NavigationEntry::SetTitle() below directly when this entry is known not to
  // be visible.
  virtual void SetTitle(std::u16string title) = 0;
  virtual const std::u16string& GetTitle() = 0;

  // The app title as set by the page. SetAppTitle gets called only if page has
  // an app-title meta tag. For all other pages, the app_title will not be set.
  // This information is provided by an experimental meta tag. See:
  // https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/DocumentSubtitle/explainer.md
  virtual void SetAppTitle(const std::u16string& app_title) = 0;

  // If app-title meta tag is set by the page, GetAppTitle will return the
  // value set by the page, including empty string. If the page does not have an
  // app-title meta tag, GetAppTitle will return a nullopt.
  virtual const std::optional<std::u16string>& GetAppTitle() = 0;

  // Page state is an opaque blob created by Blink that represents the state of
  // the page. This includes form entries and scroll position for each frame.
  // We store it so that we can supply it back to Blink to restore form state
  // properly when the user goes back and forward. |context| is an opaque object
  // that tracks FrameNavigationEntries as they are created during page state
  // initialization, and ensures equal entries are merged and shared.
  //
  // NOTE: This state is saved to disk and used to restore previous states.  If
  // the format is modified in the future, we should still be able to deal with
  // older versions.
  virtual void SetPageState(const blink::PageState& state,
                            NavigationEntryRestoreContext* context) = 0;
  virtual blink::PageState GetPageState() = 0;

  // Page-related helpers ------------------------------------------------------

  // Returns the title to be displayed on the tab. This could be the title of
  // the page if it is available or the simplified URL.
  virtual const std::u16string& GetTitleForDisplay() = 0;

  // Returns true if the current tab is in view source mode. This will be false
  // if there is no navigation.
  virtual bool IsViewSourceMode() = 0;

  // Tracking stuff ------------------------------------------------------------

  // The transition type indicates what the user did to move to this page from
  // the previous page.
  virtual void SetTransitionType(ui::PageTransition transition_type) = 0;
  virtual ui::PageTransition GetTransitionType() = 0;

  // The user typed URL was the URL that the user initiated the navigation
  // with, regardless of any redirects. This is used to generate keywords, for
  // example, based on "what the user thinks the site is called" rather than
  // what it's actually called. For example, if the user types "foo.com", that
  // may redirect somewhere arbitrary like "bar.com/foo", and we want to use
  // the name that the user things of the site as having.
  //
  // This URL will be is_empty() if the URL was navigated to some other way.
  // Callers should fall back on using the regular or display URL in this case.
  virtual const GURL& GetUserTypedURL() = 0;

  // Post data is form data that was posted to get to this page. The data will
  // have to be reposted to reload the page properly. This flag indicates
  // whether the page had post data.
  //
  // The actual post data is stored either in
  // 1) post_data when a new post data request is started.
  // 2) PageState when a post request has started and is extracted by
  //    WebKit to actually make the request.
  virtual void SetHasPostData(bool has_post_data) = 0;
  virtual bool GetHasPostData() = 0;

  // The Post identifier associated with the page.
  virtual void SetPostID(int64_t post_id) = 0;
  virtual int64_t GetPostID() = 0;

  // Holds the raw post data of a post request.
  // For efficiency, this should be cleared when PageState is populated
  // since the data is duplicated.
  // Note, this field:
  // 1) is not persisted in session restore.
  // 2) is shallow copied with the static copy Create method above.
  // 3) may be nullptr so check before use.
  virtual void SetPostData(
      const scoped_refptr<network::ResourceRequestBody>& data) = 0;
  virtual scoped_refptr<network::ResourceRequestBody> GetPostData() = 0;

  // The favicon data and tracking information. See content::FaviconStatus.
  virtual FaviconStatus& GetFavicon() = 0;

  // All the SSL flags and state. See content::SSLStatus.
  virtual SSLStatus& GetSSL() = 0;

  // Store the URL that caused this NavigationEntry to be created.
  virtual void SetOriginalRequestURL(const GURL& original_url) = 0;
  virtual const GURL& GetOriginalRequestURL() = 0;

  // Store whether or not we're overriding the user agent.
  virtual void SetIsOverridingUserAgent(bool override_ua) = 0;
  virtual bool GetIsOverridingUserAgent() = 0;

  // The time at which the last known local navigation has
  // completed. (A navigation can be completed more than once if the
  // page is reloaded.)
  //
  // If GetTimestamp() returns a null time, that means that either:
  //
  //   - this navigation hasn't completed yet;
  //   - this navigation was restored and for some reason the
  //     timestamp wasn't available;
  //   - or this navigation was copied from a foreign session.
  virtual void SetTimestamp(base::Time timestamp) = 0;
  virtual base::Time GetTimestamp() = 0;

  // Used to specify if this entry should be able to access local file://
  // resources.
  virtual void SetCanLoadLocalResources(bool allow) = 0;
  virtual bool GetCanLoadLocalResources() = 0;

  // The status code of the last known successful navigation.  If
  // GetHttpStatusCode() returns 0 that means that either:
  //
  //   - this navigation hasn't completed yet;
  //   - a response wasn't received;
  //   - or this navigation was restored and for some reason the
  //     status code wasn't available.
  virtual void SetHttpStatusCode(int http_status_code) = 0;
  virtual int GetHttpStatusCode() = 0;

  // The redirect chain traversed during this navigation, from the initial
  // redirecting URL to the final non-redirecting current URL.
  virtual void SetRedirectChain(const std::vector<GURL>& redirects) = 0;
  virtual const std::vector<GURL>& GetRedirectChain() = 0;
  // When a history entry is replaced (e.g. history.replaceState()), this
  // contains some information about the entry prior to being replaced. Even if
  // an entry is replaced multiple times, it represents data prior to the
  // *first* replace.
  virtual const std::optional<ReplacedNavigationEntryData>&
  GetReplacedEntryData() = 0;

  // True if this entry is restored and hasn't been loaded.
  virtual bool IsRestored() = 0;

  // Returns the extra headers (separated by \r\n) to send during the request.
  virtual std::string GetExtraHeaders() = 0;

  // Adds more extra headers (separated by \r\n) to send during the request.
  virtual void AddExtraHeaders(const std::string& extra_headers) = 0;

  // Returns a unique value identifying the main document for this navigation.
  // This persists across same-document navigations and stays the same after
  // a history navigation to an already visited document.
  virtual int64_t GetMainFrameDocumentSequenceNumber() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_ENTRY_H_
