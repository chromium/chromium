// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_H_

#import <UIKit/UIKit.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/blink_buildflags.h"
#include "components/sessions/core/session_id.h"
#include "ios/web/public/js_messaging/content_world.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/web_state_id.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class GURL;

@class CRWSessionStorage;
@protocol CRWScrollableContent;
@protocol CRWWebViewDownload;
@protocol CRWFindInteraction;
@protocol CRWWebViewDownloadDelegate;
@protocol CRWWebViewProxy;
typedef id<CRWWebViewProxy> CRWWebViewProxyType;
@class UIView;
typedef UIView<CRWScrollableContent> CRWContentView;

namespace web {
namespace proto {
class WebStateStorage;
class WebStateMetadataStorage;
}  // namespace proto

class BrowserState;
struct FaviconStatus;
class NavigationManager;
enum Permission : NSUInteger;
enum PermissionState : NSUInteger;
class SessionCertificatePolicyCache;
class WebFramesManager;
class WebStateDelegate;
class WebStateObserver;
class WebStatePolicyDecider;

// Normally it would be a bug for multiple WebStates to be realized in quick
// succession. However, there are some specific use cases where this is
// expected. In these scenarios call IgnoreOverRealizationCheck() before
// each expected -ForceRealized.
void IgnoreOverRealizationCheck();

// Core interface for interaction with the web.
class WebState : public base::SupportsUserData {
 public:
  // Callback used to load the full information for the WebState when
  // it will become realized.
  using WebStateStorageLoader = base::OnceCallback<proto::WebStateStorage()>;

  // Callback used to fetch the native session for the WebState.
  using NativeSessionFetcher = base::OnceCallback<NSData*()>;

  // Parameters for the Create() method.
  struct CreateParams {
    explicit CreateParams(web::BrowserState* browser_state);
    ~CreateParams();

    // The corresponding BrowserState for the new WebState.
    raw_ptr<web::BrowserState> browser_state;

    // Whether the WebState is created as the result of a window.open or by
    // clicking a link with a blank target.  Used to determine whether the
    // WebState is allowed to be closed via window.close().
    bool created_with_opener;

#if BUILDFLAG(USE_BLINK)
    // If `created_with_opener`, a pointer to the opener WebState.
    raw_ptr<WebState> opener_web_state = nullptr;
#endif

    // Value used to set the last time the WebState was made active; this
    // is the value that will be returned by GetLastActiveTime(). If this
    // is left default initialized, then the value will not be passed on
    // to the WebState and GetLastActiveTime() will return the WebState's
    // creation time.
    std::optional<base::Time> last_active_time;
  };

  // Parameters for the OpenURL() method.
  struct OpenURLParams {
    OpenURLParams(const GURL& url,
                  const GURL& virtual_url,
                  const Referrer& referrer,
                  WindowOpenDisposition disposition,
                  ui::PageTransition transition,
                  bool is_renderer_initiated);
    OpenURLParams(const GURL& url,
                  const Referrer& referrer,
                  WindowOpenDisposition disposition,
                  ui::PageTransition transition,
                  bool is_renderer_initiated);
    OpenURLParams(const OpenURLParams& params);
    OpenURLParams& operator=(const OpenURLParams& params);
    OpenURLParams(OpenURLParams&& params);
    OpenURLParams& operator=(OpenURLParams&& params);
    ~OpenURLParams();

    // The URL/virtualURL/referrer to be opened.
    GURL url;
    GURL virtual_url;
    Referrer referrer;

    // The disposition requested by the navigation source.
    WindowOpenDisposition disposition;

    // The transition type of navigation.
    ui::PageTransition transition;

    // Whether this navigation is initiated by the renderer process.
    bool is_renderer_initiated;
  };

  // InterfaceBinder can be instantiated by subclasses of WebState and returned
  // by GetInterfaceBinderForMainFrame().
  class InterfaceBinder {
   public:
    explicit InterfaceBinder(WebState* web_state);

    InterfaceBinder(const InterfaceBinder&) = delete;
    InterfaceBinder& operator=(const InterfaceBinder&) = delete;

    ~InterfaceBinder();

    template <typename Interface>
    void AddInterface(
        base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>
            callback) {
      AddInterface(
          Interface::Name_,
          base::BindRepeating(&WrapCallback<Interface>, std::move(callback)));
    }

    // Adds a callback to bind an interface receiver pipe carried by a
    // GenericPendingReceiver.
    using Callback =
        base::RepeatingCallback<void(mojo::GenericPendingReceiver*)>;
    void AddInterface(std::string_view interface_name, Callback callback);

    // Removes a callback added by AddInterface.
    void RemoveInterface(std::string_view interface_name);

    // Attempts to bind `receiver` by matching its interface name against the
    // callbacks registered on this InterfaceBinder.
    void BindInterface(mojo::GenericPendingReceiver receiver);

   private:
    template <typename Interface>
    static void WrapCallback(
        base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>
            callback,
        mojo::GenericPendingReceiver* receiver) {
      if (auto typed_receiver = receiver->As<Interface>())
        callback.Run(std::move(typed_receiver));
    }

    const raw_ptr<WebState> web_state_;
    std::map<std::string, Callback> callbacks_;
  };

  // Creates a new WebState.
  static std::unique_ptr<WebState> Create(const CreateParams& params);

  // Creates a new WebState from a serialized representation of the session.
  // `session_storage` must not be nil.
  // TODO(crbug.com/40245950): remove when the optimised serialisation feature
  // has been fully launched.
  static std::unique_ptr<WebState> CreateWithStorageSession(
      const CreateParams& params,
      CRWSessionStorage* session_storage,
      NativeSessionFetcher session_fetcher);

  // Creates a new WebState from a serialized representation of the session.
  // The callbacks are used to load the complete serialized data from disk
  // when the WebState transition to the realized state.
  static std::unique_ptr<WebState> CreateWithStorage(
      BrowserState* browser_state,
      WebStateID unique_identifier,
      proto::WebStateMetadataStorage metadata,
      WebStateStorageLoader storage_loader,
      NativeSessionFetcher session_fetcher);

  WebState(const WebState&) = delete;
  WebState& operator=(const WebState&) = delete;

  ~WebState() override {}

  // Serializes the object to `storage`. It is an error to call this method
  // on a WebState that is not realized.
  virtual void SerializeToProto(proto::WebStateStorage& storage) const = 0;

  // Serializes the object metadata to `storage`. It is valid to call this
  // method on an unrealized WebState.
  virtual void SerializeMetadataToProto(
      proto::WebStateMetadataStorage& storage) const = 0;

  // Gets/Sets the delegate.
  virtual WebStateDelegate* GetDelegate() = 0;
  virtual void SetDelegate(WebStateDelegate* delegate) = 0;

  // Clones the current WebState. The newly returned WebState is realized and
  // is a copy to the current instance but will have distinct identifiers. It
  // is used to implement prerendering or preview as this allow to have a new
  // WebState that shares the same navigation history.
  virtual std::unique_ptr<WebState> Clone() const = 0;

  // Returns whether the WebState is realized.
  //
  // What does "realized" mean? When creating a WebState from session storage
  // with `CreateWithStorageSession()` or `CreateWithStorage()`, it may not
  // yet have been fully created. Instead, it has all information to fully
  // instantiate it and its history available, but the underlying objects
  // (WKWebView, NavigationManager, ...) have not been created.
  //
  // This is an optimisation to reduce the amount of memory consumed by tabs
  // that have been restored after the browser has been shutdown. If the user
  // has many tabs, but only consult a subset of them, then there is no point
  // in creating them eagerly at startup. Instead, the creation is delayed
  // until the tabs are activated by the user.
  //
  // When the WebState becomes realized, the WebStateRealized() event will be
  // sent to all its WebStateObservers. They can listen to that event if they
  // need to support this optimisation (by delaying the creation of their own
  // state until the WebState is really used).
  //
  // See //docs/ios/unrealized_web_state.md for more information.
  virtual bool IsRealized() const = 0;

  // Forcefully bring the WebState in "realized" state. This method can safely
  // be called multiple time on a WebState, though it should not be necessary
  // to call it as the WebState will lazily switch to "realized" state when
  // needed.
  //
  // Returns `this` so that the method can be chained such as:
  //
  //    WebState* web_state = ...;
  //    web_state->ForceRealized()->SetDelegate(this);
  virtual WebState* ForceRealized() = 0;

  // Whether or not a web view is allowed to exist in this WebState. Defaults
  // to false; this should be enabled before attempting to access the view.
  virtual bool IsWebUsageEnabled() const = 0;
  virtual void SetWebUsageEnabled(bool enabled) = 0;

  // The view containing the contents of the current web page. If the view has
  // been purged due to low memory, this will recreate it. It is up to the
  // caller to size the view.
  virtual UIView* GetView() = 0;

  // Notifies the WebState that the WebContent is covered. Triggers
  // visibilitychange event.
  virtual void DidCoverWebContent() = 0;
  // Notifies the WebState that the WebContent is no longer covered. Triggers
  // visibilitychange event.
  virtual void DidRevealWebContent() = 0;

  // Get the last time that the WebState was made active (either when it was
  // created or shown with WasShown()).
  virtual base::Time GetLastActiveTime() const = 0;

  // Get the creation time of the WebState.
  virtual base::Time GetCreationTime() const = 0;

  // Must be called when the WebState becomes shown/hidden.
  virtual void WasShown() = 0;
  virtual void WasHidden() = 0;

  // When `true`, attempt to prevent the WebProcess from suspending. Embedder
  // must override WebClient::GetWindowedContainer to maintain this
  // functionality.
  virtual void SetKeepRenderProcessAlive(bool keep_alive) = 0;

  // Gets the BrowserState associated with this WebState. Can never return null.
  virtual BrowserState* GetBrowserState() const = 0;

  // Returns a weak pointer.
  virtual base::WeakPtr<WebState> GetWeakPtr() = 0;

  // Opens a URL with the given disposition.  The transition specifies how this
  // navigation should be recorded in the history system (for example, typed).
  virtual void OpenURL(const OpenURLParams& params) = 0;

  // Loads the web content from the HTML you provide as if the HTML were the
  // response to the request.
  virtual void LoadSimulatedRequest(const GURL& url,
                                    NSString* response_html_string) = 0;

  // Loads the web content from the data you provide as if the data were the
  // response to the request.
  virtual void LoadSimulatedRequest(const GURL& url,
                                    NSData* response_data,
                                    NSString* mime_type) = 0;

  // Stops any pending navigation.
  virtual void Stop() = 0;

  // Gets the NavigationManager associated with this WebState. Will return null
  // iff the WebState is unrealized. It doesn't force the realization.
  virtual const NavigationManager* GetNavigationManager() const = 0;
  // Gets the NavigationManager associated with this WebState. Can never return
  // null. It forces the realization if needed.
  virtual NavigationManager* GetNavigationManager() = 0;

  // Gets the WebFramesManager associated with this WebState. Can never return
  // null.
  virtual WebFramesManager* GetPageWorldWebFramesManager() = 0;

  // Returns the WebFrameManagerImpl associated with this WebState for the given
  // `world`.
  virtual WebFramesManager* GetWebFramesManager(ContentWorld world) = 0;

  // Gets the SessionCertificatePolicyCache for this WebState.  Can never return
  // null.
  virtual const SessionCertificatePolicyCache*
  GetSessionCertificatePolicyCache() const = 0;
  virtual SessionCertificatePolicyCache* GetSessionCertificatePolicyCache() = 0;

  // Creates a serializable representation of the session. The returned value
  // is autoreleased.
  virtual CRWSessionStorage* BuildSessionStorage() const = 0;

  // Loads `data` of type `mime_type` and replaces last committed URL with the
  // given `url`.
  virtual void LoadData(NSData* data, NSString* mime_type, const GURL& url) = 0;

  // Asynchronously executes `javaScript` in the main frame's context,
  // registering user interaction.
  virtual void ExecuteUserJavaScript(NSString* javaScript) = 0;

  // Returns a unique identifier for this WebState that is stable across
  // restart of the application (and across "undo" after a tab is closed).
  // It is local to the device and not synchronized. This can be used as a key
  // to identify locally this WebState (e.g. can be used as part of the name
  // of the file that is used to store a snapshot of the WebState, or it can
  // be used as a key in an NSDictionary).
  //
  // DEPRECATED: use GetUniqueIdentifier() instead.
  virtual NSString* GetStableIdentifier() const = 0;

  // Returns a unique identifier for this WebState that is stable across
  // restart of the application (and across "undo" after a tab is closed).
  // It is local to the device and not synchronized (but may be used by
  // the sync code to uniquely identify a session on the current device).
  // It can be used as a key to identify this WebState.
  virtual WebStateID GetUniqueIdentifier() const = 0;

  // Gets the contents MIME type.
  virtual const std::string& GetContentsMimeType() const = 0;

  // Returns true if the current page is a web view with HTML.
  virtual bool ContentIsHTML() const = 0;

  // Returns the current navigation title. This could be the title of the page
  // if it is available or the URL.
  virtual const std::u16string& GetTitle() const = 0;

  // Returns true if the current page is loading.
  virtual bool IsLoading() const = 0;

  // The fraction of the page load that has completed as a number between 0.0
  // (nothing loaded) and 1.0 (fully loaded).
  virtual double GetLoadingProgress() const = 0;

  // Whether the WebState is visible. Returns true after WasShown() call and
  // false after WasHidden() call.
  virtual bool IsVisible() const = 0;

  // Returns true if the web process backing this WebState is believed to
  // currently be crashed.
  virtual bool IsCrashed() const = 0;

  // Returns true if the web process backing this WebState is believed to
  // currently be crashed or was evicted (by calling SetWebUsageEnabled
  // with false).
  // TODO(crbug.com/41258826): Remove once all code has been ported to use
  // IsCrashed() instead of IsEvicted().
  virtual bool IsEvicted() const = 0;

  // Whether this instance is in the process of being destroyed.
  virtual bool IsBeingDestroyed() const = 0;

  // Whether this instance's web page is in fullscreen mode.
  virtual bool IsWebPageInFullscreenMode() const = 0;

  // Gets/Sets the favicon for the current page displayed by this WebState.
  virtual const FaviconStatus& GetFaviconStatus() const = 0;
  virtual void SetFaviconStatus(const FaviconStatus& favicon_status) = 0;

  // Returns the number of items in the NavigationManager, excluding
  // pending entries.
  // TODO(crbug.com/40436539): Update to return size_t.
  virtual int GetNavigationItemCount() const = 0;

  // Gets the URL currently being displayed in the URL bar, if there is one.
  // This URL might be a pending navigation that hasn't committed yet, so it is
  // not guaranteed to match the current page in this WebState. A typical
  // example of this is interstitials, which show the URL of the new/loading
  // page (active) but the security context is of the old page (last committed).
  virtual const GURL& GetVisibleURL() const = 0;

  // Gets the last committed URL. It represents the current page that is
  // displayed in this WebState. It represents the current security context.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Returns the last committed URL if the correctness of this URL's origin is
  // trusted, and std::nullopt otherwise.
  virtual std::optional<GURL> GetLastCommittedURLIfTrusted() const = 0;

  // Returns the current CRWWebViewProxy object.
  virtual CRWWebViewProxyType GetWebViewProxy() const = 0;

  // Typically an embedder will:
  //    - Implement this method to receive notification of changes to the page's
  //      `VisibleSecurityState`, updating security UI (e.g. a lock icon) to
  //      reflect the current security state of the page.
  // ...and optionally:
  //    - Invoke this method upon detection of an event that will change
  //      the security state (e.g. a non-secure form element is edited).
  virtual void DidChangeVisibleSecurityState() = 0;

  virtual InterfaceBinder* GetInterfaceBinderForMainFrame();

  // Whether this WebState was created with an opener.
  // See CreateParams::created_with_opener for more details.
  virtual bool HasOpener() const = 0;
  virtual void SetHasOpener(bool has_opener) = 0;

  // Callback used to handle snapshots. The parameter is the snapshot image.
  typedef base::RepeatingCallback<void(UIImage*)> SnapshotCallback;

  // Returns whether TakeSnapshot() can be executed.  The API may be disabled if
  // the WKWebView IPC mechanism is blocked due to an outstanding JavaScript
  // dialog.
  virtual bool CanTakeSnapshot() const = 0;

  // Takes a snapshot of this WebState with `rect`. `rect` should be specified
  // in the coordinate system of the view returned by GetView(). `callback` is
  // asynchronously invoked after performing the snapshot. Prior to iOS 11, the
  // callback is invoked with a nil snapshot.
  virtual void TakeSnapshot(const CGRect rect, SnapshotCallback callback) = 0;

  // Creates PDF representation of the web page and invokes the `callback` with
  // the NSData of the PDF or nil if a PDF couldn't be generated.
  virtual void CreateFullPagePdf(
      base::OnceCallback<void(NSData*)> callback) = 0;

  // Tries to dismiss the presented states of the media (fullscreen or Picture
  // in Picture).
  virtual void CloseMediaPresentations() = 0;

  // Adds and removes observers for page navigation notifications. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  virtual void AddObserver(WebStateObserver* observer) = 0;
  virtual void RemoveObserver(WebStateObserver* observer) = 0;

  // Instructs the delegate to close this web state. Called when the page calls
  // wants to close self by calling window.close() JavaScript API.
  virtual void CloseWebState() = 0;

  // Injects an opaque NSData block into a WKWebView to restore or serialize.
  // Returns true if this operation succeeds, and false otherwise.
  virtual bool SetSessionStateData(NSData* data) = 0;
  virtual NSData* SessionStateData() = 0;

  // Gets or sets the web state's permission for a specific type, for example
  // camera or microphone, on the device.
  virtual PermissionState GetStateForPermission(
      Permission permission) const = 0;
  virtual void SetStateForPermission(PermissionState state,
                                     Permission permission) = 0;

  // Gets a mapping of all available permissions and their states.
  // Note that both key and value are in NSNumber format, and should be
  // translated to NSUInteger and casted to web::Permission or
  // web::PermissionState before use.
  virtual NSDictionary<NSNumber*, NSNumber*>* GetStatesForAllPermissions()
      const = 0;

  // Downloads the displayed webview at `destination_file`. `handler`
  // is used to retrieve the CRWWebViewDownload, so the caller can manage the
  // launched download.
  virtual void DownloadCurrentPage(NSString* destination_file,
                                   id<CRWWebViewDownloadDelegate> delegate,
                                   void (^handler)(id<CRWWebViewDownload>)) = 0;

  // Whether the Find interaction is supported and can be enabled.
  virtual bool IsFindInteractionSupported() = 0;

  // Whether the Find interaction in the contained web view is enabled. Should
  // only be called if `IsFindInteractionSupported()` returns `true`.
  virtual bool IsFindInteractionEnabled() = 0;

  // Enable or disable the Find interaction for the contained web view. Should
  // only be called if `IsFindInteractionSupported()` returns `true`.
  virtual void SetFindInteractionEnabled(bool enabled) = 0;

  // Get the Find interaction object associated with the contained web view.
  // Returns `nil` if the Find interaction is currently disabled. Should only be
  // called if `IsFindInteractionSupported()` returns `true`.
  virtual id<CRWFindInteraction> GetFindInteraction() = 0;

  // Get an opaque activity item that can be passed to a
  // UIActivityViewController to share the current URL.
  virtual id GetActivityItem() API_AVAILABLE(ios(16.4)) = 0;

  // Returns the page theme color.
  virtual UIColor* GetThemeColor() = 0;

  // Returns the under page background color.
  virtual UIColor* GetUnderPageBackgroundColor() = 0;

 protected:
  friend class WebStatePolicyDecider;

  // Adds and removes policy deciders for navigation actions. The order in which
  // deciders are called is undefined, and will stop on the first decider that
  // refuses a navigation. Clients must be sure to remove the deciders before
  // they go away.
  virtual void AddPolicyDecider(WebStatePolicyDecider* decider) = 0;
  virtual void RemovePolicyDecider(WebStatePolicyDecider* decider) = 0;

  WebState() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_H_
