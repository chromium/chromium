# Browser Controls

The browser controls system provides a way to move the core browser UI synchronously with the changes in the web content size and offset. The primary goal of this system is to maximize the web content the user can see by hiding the UI components smoothly when the user is scrolling through the website and showing them smoothly when the user needs them again. The interactions of the Java Android views (the core UI) with the web content are accommodated by this system. The browser controls system is dynamic, and it interacts with the web contents to position the controls.  This is one of the reasons why these seemingly simple browser controls are complicated and difficult to work with.

[TOC]

## Android views and composited textures

There are two components to the browser controls: an Android view that handles the user interaction and a composited texture that is used for scrolling. When the browser controls are fully visible on the screen, what the user sees is an Android view, but the moment the user starts scrolling down on the web page, the Android views are hidden (i.e. visibility set to View#INVISIBLE), exposing the visually identical composited texture behind it. The composited texture follows the web contents throughout the scrolling gesture and can be scrolled off the screen completely.

### Why a composited texture?

The browser controls need to be in sync with the web contents when scrolling. Android views and web contents are drawn on different surfaces that are updated at different times. So, even if we were to observe the web contents and update the Android view positions accordingly (i.e. View#setTranslationY()), the browser controls would still be out of sync with the web contents.  Android 10 provides a [SurfaceControl](https://developer.android.com/about/versions/10/features#surface) API that might solve this issue without having to use composited textures, but this is unlikely to be adopted anytime soon since it’s only available on Q+.

### How does it work?

There are several classes that are used to draw composited textures:
- [ViewResourceFrameLayout](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/ViewResourceFrameLayout.java): A view group that can easily be transformed into a texture used by the compositor system.
- [SceneLayer (native)](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/layouts/scene_layer.h): A wrapper that provides a [cc::Layer](https://source.chromium.org/chromium/chromium/src/+/main:cc/layers/layer.h) and dictates how that layout is supposed to interact with the layout system.
- [SceneLayer](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/layouts/java/src/org/chromium/chrome/browser/layouts/scene_layer/SceneLayer.java): The Java representation of SceneLayer.
- [SceneOverlay](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/layouts/java/src/org/chromium/chrome/browser/layouts/SceneOverlay.java): An interface that allows for other texture-like things to be drawn on a layout without being a layout itself.
- [SceneOverlayLayer](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/layouts/java/src/org/chromium/chrome/browser/layouts/scene_layer/SceneOverlayLayer.java): Extends SceneLayer for SceneOverlay.
- [LayoutManager](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/layouts/java/src/org/chromium/chrome/browser/layouts/LayoutManager.java): The class that manages the Layouts. In the browser controls context, this is the manager that adds the SceneOverlay to the Layout to be drawn.

The Android view is wrapped in a `ViewResourceFrameLayout`. If we look at [bottom_control_container.xml](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/toolbar/java/res/layout/bottom_control_container.xml), the xml layout for the bottom controls, the views are wrapped in [ScrollingBottomViewResourceFrameLayout](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/bottom/ScrollingBottomViewResourceFrameLayout.java).

A scene layer ([ScrollingBottomViewSceneLayer](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/bottom/ScrollingBottomViewSceneLayer.java) and its native counterpart [scrolling_bottom_view_scene_layer.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/android/compositor/scene_layer/scrolling_bottom_view_scene_layer.cc) in this example) is responsible for creating a compositor layer using the view resource. [LayoutManager](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/compositor/layouts/LayoutManager.java) adds the scene layer to the global layout.

See these example CLs [adding a scene layer](https://chromium-review.googlesource.com/c/chromium/src/+/1769631) and [adding the Android view to use as a resource](https://chromium-review.googlesource.com/c/chromium/src/+/1809813).

## How does the browser controls system work?

This system spans across two processes: browser and renderer. The browser process controls some properties of the browser controls such as height and min-height and provides them to the renderer. The renderer manages the positioning of the controls in response to the scroll events from the user and changes in the properties provided by the browser process, then sends the browser controls state information to the browser process. In cases where it isn’t possible to have a renderer process manage the positioning of the controls, the browser process also manages the positioning. See [here](#browser-controls-when-there-is-no-web-contents).

Browser controls properties:
- {Top|Bottom}ControlsHeight: Total height of the controls.
- {Top|Bottom}ControlsMinHeight: Minimum visible height of the controls.
- AnimateBrowserControlsHeightChanges: Whether the changes in browser controls heights should be animated.

Browser controls state/positioning information:
- {Top|Bottom}ControlOffset: Vertical displacement of the controls relative to their resting (or fully shown) position.
- {Top|Bottom}ContentOffset: Content offset from the edge of the screen or the visible height of the controls.
- {Top|Bottom}ControlsMinHeightOffset: Current min-height, useful when the min-height is changed with animation.

### Browser controls in the browser process

[BrowserControlsManager](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java) is the class that manages the browser controls properties and responds to the browser controls related information that comes from the renderer process. It provides an interface, [BrowserControlsStateProvider](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/browser_controls/android/java/src/org/chromium/chrome/browser/browser_controls/BrowserControlsStateProvider.java), that can be used to get/set browser controls properties. BrowserControlsStateProvider in addition provides an Observer interface for observing changes in browser controls properties and offsets.

### Browser controls in the renderer: cc and Blink

There are 2 classes that are responsible for calculating the browser controls offsets in the renderer: [cc::BrowserControlsOffsetManager](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/browser_controls_offset_manager.cc) and [blink::BrowserControls](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/browser_controls_offset_manager.cc). These two classes are almost identical in behavior except for some small differences:

- Both classes calculate browser controls offsets during scroll events. Both classes have functions ScrollBegin(), ScrollBy(), and ScrollEnd() that notify the class of the changes in scrolling state. The biggest difference here is that cc::BrowserControlsOffsetManager calculates the offsets when the scroll happens on the cc thread (or impl thread, more on that in [this doc](https://source.chromium.org/chromium/chromium/src/+/main:docs/how_cc_works.md)) while blink::BrowserControls calculates the offsets for main thread scrolling. Today, most of the scrolling happens on the impl thread and the main thread scrolling is used only as a fallback.
- Animations are only handled by `cc::BrowserControlsOffsetManager`. Animation in this context means changing the browser controls offsets gradually without user action. There are two types of animations that are handled by this class:

    - Show/hide animations: All these animations do is to completely hide or show the browser controls smoothly, e.g. when a scroll event leaves the controls half shown or the browser controls state changes. Examples of state change include setting the controls state to shown when navigating to a site and setting it to hidden when a fullscreen video is playing. The browser controls states are defined in [browser_controls_state.h](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/browser_controls_state.h;drc=23f61cb65a94208dc2c4728e895e87d47f64a8b6;l=10).
    - Height-change animations: These animations make a change in browser controls height smooth. They use the same logic/path as the show/hide animations. However, height-change animations are set up in the [BrowserControlsOffsetManager::OnBrowserControlsParamsChanged](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/browser_controls_state.h;drc=23f61cb65a94208dc2c4728e895e87d47f64a8b6;l=10) function using the old and new params and the current state of the controls.

Any discrepancy between the values from the cc and the blink versions of the browser controls classes are resolved using [SyncedProperty](https://source.chromium.org/chromium/chromium/src/+/main:cc/base/synced_property.h).

#### Scrolling, ratios, offsets

`cc::BrowserControlsOffsetManager` has 3 functions responsible for handling the scroll gestures: `::ScrollBegin(), ::ScrollBy(), and ::ScrollEnd()`. `::ScrollBy()` is called with the pending scroll delta during the scroll and calculates how much of the browser controls should be visible after this scroll event. This class uses "shown ratio" instead of offsets (though it should probably be raw pixel offsets). In normal operation, the shown ratio is in the range [0, 1], 0 being completely hidden and 1 being completely visible. The only time it will be outside of this range is when computing height-change animations. Also, if the min-height is set to a value larger than 0, the lower end of this range will be updated accordingly. This is called ‘MinShownRatio’ in code and is equal to `MinHeight / ControlsHeight`.

A shown ratio is calculated for the top and bottom controls separately. Even though the top and bottom controls are currently connected, they don't have to share the same shown ratio (especially true with min height animations). The shown ratios that are calculated in the renderer process are sent to the browser process in a [RenderFrameMetadata](https://source.chromium.org/chromium/chromium/src/+/main:cc/trees/render_frame_metadata.h) struct, alongside the controls heights and min-height offsets.

These ratios are used to calculate the visible height and the controls offsets in the browser process so that the composited texture for the browser controls is displayed in the right place.

### How are the browser controls properties sent from the browser to the renderer?

Path for the top controls height (other properties follow a similar one):

- `BrowserControlsSizer#setTopControlsHeight()` (Implemented in BrowserControlsManager.java)
- Calls `BrowserControlsStateProvider#Observer#onTopControlsHeightChanged()` (Implemented in CompositorViewHolder.java)
- Calls `WebContents#notifyBrowserControlsHeightChanged()` (Implemented in WebContentsImpl.java)
- Calls `WebContentsAndroid::NotifyBrowserControlsHeightChanged()` (Native)
- Calls `ViewAndroid::OnBrowserControlsHeightChanged()`
- Calls `EventHandlerAndroid::OnBrowserControlsHeightChanged()` (Implemented in web_contents_view_android.cc)
- Calls `RenderWidgetHostViewAndroid::SynchronizeVisualProperties()`
- Calls `RenderWidgetHostImpl::SynchronizeVisualProperties()`
    - Calls `RenderWidgetHostImpl::GetVisualProperties()` to fill the `blink::VisualProperties` struct. Browser controls properties are put in the `cc::BrowserControlsParams` struct
    - To pull the browser controls properties:
        - Calls `RenderViewHostDelegateView::GetTopControlsHeight()` (implemented in web_contents_view_android.cc)
        - Calls `WebContentsDelegate::GetTopControlsHeight()` (implemented in web_contents_delegate_android.cc)
        - Calls `WebContentsDelegateAndroid#getTopControlsHeight()` (Java, implemented in ActivityTabWebContentsDelegateAndroid.java)
        - Calls `BrowserControlsStateProvider#getTopControlsHeight()` (implemented in BrowserControlsManager.java)
        - `BrowserControlsManager` is the source of truth for the browser controls properties
- Calls `blink::mojom::Widget::UpdateVisualProperties() // blink_widget_` with the filled `VisualProperties` to send the properties to the renderer.
- `WidgetBase` in blink receives the message (through the `UpdateVisualProperties` override of the widget mojo interface).
- Calls `LayerTreeHost::SetBrowserControlsParams()` (this is for the impl thread)
    - `LayerTreeHost` caches the `BrowserControlsParams` then calls `SetNeedsCommit()`.
    - When the properties are pushed to the active tree and `LayerTreeImpl::SetBrowserControlsParams` is called, `BrowserControlsOffsetManager::OnBrowserControlsParamsChanged()` is called as a result.
    - `BrowserControlsOffsetManager` then updates the shown ratios and/or sets up animations depending on the new and old properties.
- Also calls `WidgetBaseClient::UpdateVisualProperties()` (this is for the main thread)
    - Implemented in web_frame_widget_base.cc
    - Calls `WebWidgetClient` implemented in render_widget.cc
    - Calls `RenderWidget::ResizeWebWidget()`
    - Calls `RenderWidgetDelegate::ResizeWebWidgetForWidget()` implemented in render_view_impl.cc
    - Calls `WebView::ResizeWithBrowserControls()` implemented in web_view_impl.cc
    - Calls `blink::BrowserControls::SetParams()`

[Example CL that wires the browser controls params](https://crrev.com/c/1918262)

### How are the browser controls shown ratios and offsets sent from the renderer to the browser?

Path for the top controls shown ratio (other ratios, heights and offsets follow a similar one):

- When a compositor frame is being drawn, `LayerTreeHostImpl::MakeRenderFrameMetadata()` is called to get the filled `RenderFrameMetadata` struct.
- `LayerTreeHostImpl` then calls `RenderFrameMetadataObserver::OnRenderFrameSubmission()` (implemented in render_frame_metadata_observer_impl.cc) with the filled `RenderFrameMetadata` struct
- Calls `mojom::RenderFrameMetadataObserverClient::OnRenderFrameMetadataChanged` to send the metadata to the browser
- `RenderFrameMetadataProviderImpl` receives the message (through the `OnRenderFrameMetadataChanged` override of the `RenderFrameMetadataObserverClient` mojo interface)
- Calls `RenderFrameMetadataProvider::Observer::OnRenderFrameMetadataChangedBeforeActivation` overridden by `RenderWidgetHostViewBase` and implemented in render_widget_host_view_android.cc
- Calls `RenderWidgetHostViewAndroid::UpdateControls`
    - Here, the shown ratio and the controls height are converted to controls offset.
- Calls `ViewAndroid::OnTopControlsChanged()` with the calculated offsets
- Calls `ViewAndroidDelegate#onTopControlsChanged()` (Java) implemented in TabViewAndroidDelegate.java
- Calls `TabBrowserControlsOffsetHelper#setTopOffset()`
- Calls `TabObserver#onBrowserControlsOffsetChanged()` implemented in BrowserControlsManager.java

[Example CL that wires the browser controls min-height offsets](https://crrev.com/c/2003869)

## Browser controls when there is no web contents

When there is no web contents the browser controls can/should interact with, the browser controls will always be fully shown. This is currently the case for native pages such as NTP and other surfaces such as the tab switcher. However, the browser controls heights can still change, even without web contents. In this case, the transition animations are run by `BrowserControlsManager` in the browser process. See [BrowserControlsManager.java](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;drc=5b029d1b98e2b7b35fff59b50a54af5d47689de9;l=725). `BrowserControlsManager` animates the height changes by updating the controls offsets and the content offset over time. For every frame, `BrowserControlsStateProvider#Observer`s are notified through `#onControlsOffsetChanged()`.

## Making sure your surface works in harmony with the browser controls

- Surfaces where the browser controls are visible should never use a dimen resource (a common example is R.dimen.toolbar_height_no_shadow) to position its contents.
- If a surface needs to pad, margin or offset its contents, using the `BrowserControlsStateProvider` interface will be the best way most of the time.
- Surfaces should also be aware that the controls heights or the content offsets can change at any time. The `BrowserControlsStateProvider#Observer` interface can be used for this purpose. Also, a height change is very likely to be followed by content/controls offset updates over time. In this case, basing any margin/padding/offset updates on `Observer#onTopControlsHeightChanged` calls might cause a jump instead of a smooth animation.
