// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FENCED_FRAME_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_FENCED_FRAME_TEST_UTIL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/optional_ref.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"

class GURL;

namespace gfx {
class PointF;
}

namespace content {

class RenderFrameHost;
class FencedFrameURLMapping;
class FencedFrameReporter;
class ToRenderFrameHost;

namespace test {

// Browser tests can use this class to more conveniently leverage fenced frames.
class FencedFrameTestHelper {
 public:
  explicit FencedFrameTestHelper();
  ~FencedFrameTestHelper();
  FencedFrameTestHelper(const FencedFrameTestHelper&) = delete;
  FencedFrameTestHelper& operator=(const FencedFrameTestHelper&) = delete;

  // This method creates a new fenced frame in `mode` rooted at
  // `fenced_frame_parent` that is navigated to `url`. This method waits for the
  // navigation to `url` to commit, and returns the `RenderFrameHost` that
  // committed the navigation if it succeeded. Otherwise, it returns `nullptr`.
  // See `NavigationFrameInFencedFrameTree()` documentation for the
  // `expected_error_code` parameter.
  RenderFrameHost* CreateFencedFrame(
      RenderFrameHost* fenced_frame_parent,
      const GURL& url,
      net::Error expected_error_code = net::OK,
      blink::FencedFrame::DeprecatedFencedFrameMode mode =
          blink::FencedFrame::DeprecatedFencedFrameMode::kDefault,
      bool wait_for_load = true);

  // This method is similar to `FencedFrameTestHelper::CreateFencedFrame` but
  // doesn't wait until the fenced frame completes loading.
  void CreateFencedFrameAsync(RenderFrameHost* fenced_frame_parent_rfh,
                              const GURL& url);

  void NavigateFencedFrameUsingFledge(RenderFrameHost* fenced_frame_parent,
                                      const GURL& url,
                                      const std::string fenced_frame_id);

  // This method provides a way to navigate frames within a fenced frame's tree,
  // and synchronously wait for the load to finish. This method returns the
  // `RenderFrameHost` that the navigation committed to if it was successful
  // (which may be different from the one that navigation started in), and
  // `nullptr` otherwise. It takes an `expected_error_code` in case the
  // navigation to `url` fails, which can be detected on a per-error-code basis.
  // TODO(crbug.com/40820418): Directly use TestFrameNavigationObserver instead
  // of relying on this method.
  RenderFrameHost* NavigateFrameInFencedFrameTree(
      RenderFrameHost* rfh,
      const GURL& url,
      net::Error expected_error_code = net::OK,
      bool wait_for_load = true);

  // Helper function for event reporting tests that sends out a request. This is
  // used to see if this request or the actual event reached the server first.
  // If this reaches the server first, we know that the event was not sent.
  void SendBasicRequest(WebContents* web_contents,
                        GURL url,
                        std::optional<std::string> content = std::nullopt);

  // Returns the last created fenced frame. This can be used by embedders who
  // must create fenced frames from script but need to get the fence frame's
  // inner root RenderFrameHost.
  // This method will return nullptr if no fenced frames were created.
  static RenderFrameHost* GetMostRecentlyAddedFencedFrame(RenderFrameHost* rfh);

  // Returns a vector of all child fenced frames given a parent `rfh`. This can
  // be used by embedders who must create multiple fenced frames from script but
  // need to get their inner root RenderFrameHosts.
  static std::vector<RenderFrameHost*> GetChildFencedFrameHosts(
      RenderFrameHost* rfh);

  // Class for observing visibility changes in fenced frame roots, such as when
  // they enter or leave the viewport.
  class FencedFrameVisibilityObserver : public WebContentsObserver {
   public:
    // If `fenced_frame_root` is nullptr, `Wait()` will terminate when a
    // newly-created fenced frame root has `visibility`. Otherwise, `Wait()`
    // will terminate when the node specified by `fenced_frame_root` has
    // `visibility`.
    explicit FencedFrameVisibilityObserver(
        WebContents* web_contents,
        blink::mojom::FrameVisibility visibility,
        RenderFrameHost* fenced_frame_root);

    // WebContentsObserver implementation.
    void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
    void RenderFrameHostChanged(RenderFrameHost* old_host,
                                RenderFrameHost* new_host) override;
    void OnFrameVisibilityChanged(
        RenderFrameHost* rfh,
        blink::mojom::FrameVisibility visibility) override;

    void Wait();

   private:
    blink::mojom::FrameVisibility target_visibility_ =
        blink::mojom::FrameVisibility::kNotRendered;
    raw_ptr<RenderFrameHost> fenced_frame_root_;
    base::RunLoop run_loop_;
  };

  // Creates a fenced frame and waits for it to be rendered in the viewport.
  // If `url` is provided, also navigates the fenced frame to the provided URL,
  // and waits for it to load successfully. Returns the root RenderFrameHost of
  // the fenced frame's inner FrameTree.
  RenderFrameHost* CreateFencedFrameAndWaitUntilRenderedInViewport(
      RenderFrameHost* parent_rfh,
      base::optional_ref<const GURL> url);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Maps a URL to a URN that can be loaded into an opaque-ads fenced frame. This
// can be called from outside of `content/` as it does not require access to
// `RenderFrameHostImpl`.
GURL CreateFencedFrameURLMapping(RenderFrameHost* rfh, const GURL& url);

// Helper function that converts a URL to a URN and adds the mapping to a given
// fenced frame URL mapping object. This can only be called from inside of
// `content/`.
GURL AddAndVerifyFencedFrameURL(
    FencedFrameURLMapping* fenced_frame_url_mapping,
    const GURL& https_url,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter = nullptr);

// Revoke the fenced frame untrusted network access.
bool RevokeFencedFrameUntrustedNetwork(RenderFrameHost* rfh);

// Exempt the `urls` from fenced frame untrusted network revocation.
void ExemptUrlsFromFencedFrameNetworkRevocation(RenderFrameHost* rfh,
                                                const std::vector<GURL>& urls);

// Create and set fenced frame config to map to the given URL.
void SetFencedFrameConfig(RenderFrameHost* rfh, const GURL& url);

// Simulate a mouse click at the `point` inside fenced frame tree.
//
// Note: if the click is inside a nested iframe which is same-site with the
// root fenced frame, the coordinates of `point` need to be offset by the
// top-left coordinates of the nested iframe relative to the fenced frame. See
// `GetTopLeftCoordinatesOfElementWithId`. This is because the same-site nested
// iframe does not have a `RenderWidgetHost`. The mouse event is forwarded to
// the `RenderWidgetHost` of the fenced frame even if the nested iframe
// `RenderFrameHost` is passed in.
//
// However, a cross-site nested iframe becomes an OOPIF in fenced frame tree and
// gets its own `RenderWidgetHost`. There is no need to offset the coordinates.
//
// Note: If the simulation does not take place inside a fenced frame tree,
// use `SimulateMouseClickAt` in `content/public/test/browser_test_utils.h`.
void SimulateClickInFencedFrameTree(const ToRenderFrameHost& adapter,
                                    blink::WebMouseEvent::Button button,
                                    const gfx::PointF& point);

// Get the top left coordinates of the element with `id`, relative to `adapter`.
gfx::PointF GetTopLeftCoordinatesOfElementWithId(
    const ToRenderFrameHost& adapter,
    std::string_view id);

}  // namespace test

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FENCED_FRAME_TEST_UTIL_H_
