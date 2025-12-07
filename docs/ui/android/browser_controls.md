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

There are two paths involved:
- The default path, which spans across two processes (browser and renderer) and is responsible for moving the browser controls when they are not scrollable.
- The fast path, which spans across all three processes (browser, renderer and gpu) and is responsible for moving the browser controls when they are scrollable (see Browser controls in viz section below for more details)
The browser process controls some properties of the browser controls such as height and min-height and provides them to the renderer. The renderer manages the positioning of the controls in response to the scroll events from the user and changes in the properties provided by the browser process. In the default path, the browser controls state information is sent to the browser process. In the fast path, some information is also sent to the gpu. In cases where it isn’t possible to have a renderer process manage the positioning of the controls, the browser process also manages the positioning. See [here](#browser-controls-when-there-is-no-web-contents).

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

## Browser controls in viz (BCIV)

This is a new fast path for scrolling/animating browser controls that made scrolling more responsive and less janky, and launched in June 2025. A high level description of individual components/items are provided first. A detailed sequence of events that happen during scrolls and animations will follow.

### Summarizing what happens in the default path

1. Browser handles touch events, and dispatches them to the renderer.
2. Renderer calculates the new offset, submits a frame that updates the position of the content layer and notifies the browser of the new offset.
3. Browser updates its property models with the new offset and submits a frame that updates the positions of the browser controls.
4. Viz draws once it receives both frames.

### Core items to implement for BCIV

#### Capturing earlier

[Capturing](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/resources/dynamics/ViewResourceAdapter.java;l=52;drc=2d6c94396f33fe6c4a95c11db6c48f45302a4ab2;bpv=1;bpt=1) refers to converting an android view to a bitmap. While scrolling, parts of the composited UI pull [resources](https://source.chromium.org/chromium/chromium/src/+/main:cc/layers/ui_resource_layer.h;l=33?q=uiresourcelayer&ss=chromium%2Fchromium%2Fsrc) from the captured bitmap, which determines what that layer draws to the screen. This means we must have a capture of the most recent state of the browser controls before scrolling. Otherwise, the screen might display an older version of the UI (ex. the toolbar could show an old url instead of the current one) or a blank layer (in the case where there is no capture.)

Currently, there are cases where capturing is initiated by scrolling. But captures can be slow, so this could cause a noticeable delay in the start of the scroll. Capturing also causes a browser frame to be submitted, which doesn’t align with the goal of BCIV. To fix this problem, we capture at an earlier point in time. For the following reasons, we decide to capture when page load finishes:
- Capturing before page load finishes would introduce non-trivial work on the browser, which would delay navigation/page load.
- The visual appearance of browser controls could change before page load finishes (for example security state, theme color, optional buttons appearing, etc.) so it’s possible the first capture was wasted work and we would have to capture again.
- After page load, the controls are locked for 3 seconds. During this window, the controls aren’t scrollable, and there should be more than enough time for the capture to finish.

#### Removing surface sync

SurfaceSync is a mechanism used during scrolling to ensure that both the browser and renderer frames are received before attempting to draw. The browser updates the positions of the browser controls, while the renderer updates the position of the content layer. So we need SurfaceSync to make sure the controls and content layer move together. But when scrolling with BCIV, there won’t be a browser frame anymore, so we [don’t trigger SurfaceSync](https://source.chromium.org/chromium/chromium/src/+/main:cc/trees/layer_tree_host_impl.cc;l=2734?q=allocate_%20inviz%20f:cc&ss=chromium%2Fchromium%2Fsrc) in this case. For animations, SurfaceSync is sometimes still needed (see Stack trace for animations section below for more details.)

#### Creating and distributing OffsetTag and OffsetTagValues

An [OffsetTag](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/android/java/src/org/chromium/cc/input/OffsetTag.java;l=17?q=offsettag&ss=chromium%2Fchromium%2Fsrc) is a wrapper for an UnguessableToken. We tag a slim::Layer with an OffsetTag to indicate that viz is able to move it.

An [OffsetTagValue](https://source.chromium.org/chromium/chromium/src/+/main:components/viz/common/quads/offset_tag.h;l=65?q=offsettagvalue%20-f:out&ss=chromium%2Fchromium%2Fsrc) associates an OffsetTag with an offset, which represents how far the layers with this tag should be offset by.

 Tags need to be distributed to 3 areas:

1. The [slim::Layers](https://source.chromium.org/chromium/chromium/src/+/main:cc/slim/layer.h;l=174?q=offsettag%20f:layer%20-f:out&ss=chromium%2Fchromium%2Fsrc) of all browser UI that need to be moved by viz. This gets included in the compositor frame’s metadata when the frame is submitted to viz, which is how viz knows which layers to move. For a given tag, viz will apply the same translation to all layers with that tag. This means that layers that need to move differently will need their own unique tag (ex. bottom controls move in the opposite direction as top controls, so they must use different tags.)
2. The SurfaceLayer in [DelegatedFrameHostAndroid](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/delegated_frame_host_android.h;l=220;drc=4834462d69ddf6e0ac6b27572618e4436de4a2f3;bpv=0;bpt=1), so viz knows [what tag to look for](https://source.chromium.org/chromium/chromium/src/+/main:components/viz/service/display/surface_aggregator.cc;l=722?q=offset_tag%20f:surface_ag&ss=chromium) when [iterating through quad states](https://source.chromium.org/chromium/chromium/src/+/main:components/viz/service/display/resolved_frame_data.cc;l=400;drc=b97f6486e3ccf883f2e2f085abfb4829d8ca6f8d) in its specified SurfaceRange.
3. The renderer, which will calculate the offset for each frame during a scroll and create an [OffsetTagValue](https://source.chromium.org/chromium/chromium/src/+/main:cc/trees/layer_tree_host_impl.cc;l=2561?q=f:layer_tree%20offsettag%20metadata). This gets included in the compositor frame’s metadata when the frame is submitted to viz, which is how viz knows what translation to apply.

Currently, we have 3 OffsetTags, one tag for each group: top controls, content layer, bottom controls. As mentioned, the content layer and top controls move in the opposite direction as bottom controls, so they need to have different tags. However, due to the toolbar’s shadow, the top controls and content layer must also have a different tag. The shadow is a visual effect that is z-indexed on top of the content layer. When the toolbar is moved off screen, the shadow needs to immediately disappear. Prior to BCIV, the visibility of the shadow’s layer was toggled, but this would incur a browser frame. To accomplish this with BCIV, we just offset the toolbar additionally by the shadow’s height when it goes off screen (this is done with the bottom controls as well, since it also has a shadow.)

#### Updating OffsetTags

The existence/absence of an OffsetTag on a layer depends on whether that layer is moveable during a scroll/animation. The browser process controls whether or not the browser controls can be scrolled via [BrowserControlsVisibilityDelegate](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/util/android/java/src/org/chromium/components/browser_ui/util/BrowserControlsVisibilityDelegate.java;l=11;bpv=1;bpt=1?q=BrowserControlsVisibilityDelegate&ss=chromium%2Fchromium%2Fsrc). This is an ObservableSupplier that can be set with a [BrowserControlsState](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/browser_controls_state.h;l=12?q=browsercontrolsstate&ss=chromium%2Fchromium%2Fsrc) value:

- kShown/kHidden means the browser is forcing the toolbar to be fully shown/hidden. The browser controls are not allowed to be scrolled, so we remove all tags from all layers to prevent viz from moving them. In this state, any behavior that results in browser controls being moved will not involve BCIV.
- kBoth means the renderer is able to control the browser controls. The browser controls are able to be scrolled, so we create new tags and distribute them to the necessary areas.

These updates are done in [TabBrowserControlsConstraintsHelper](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/tab/TabBrowserControlsConstraintsHelper.java;l=287;drc=3a35ef8d20836722c95b230f7248c73faea599e7) when the visibility constraint changes.

We also update the tags when the tab becomes [hidden/shown](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/tab/TabBrowserControlsConstraintsHelper.java;l=161;drc=3a35ef8d20836722c95b230f7248c73faea599e7;bpv=0;bpt=1). This is because there are times where the BrowserControlsState changes while the tab is hidden/uninteractable (ex. when the grid tab switcher is overlaid above the UI) which results in the TabBrowserControlsConstraintsHelper not getting notified. If we don’t update the tags, the controls could either remain unscrollable, or become scrollable when it’s not supposed to be, which is a security concern.


#### OffsetTagConstraints

An [OffsetTagConstraint](https://source.chromium.org/chromium/chromium/src/+/main:components/viz/common/quads/offset_tag.h;l=76;drc=3a35ef8d20836722c95b230f7248c73faea599e7) contains a min and max for both x and y directions, which represents the valid range the layers can move in. If an OffsetTagValue indicates to move a layer outside of the valid range, the position of the layer will be clamped to the range’s boundary.

While scrolling, the constraints for each layer will be set to allow the layer to be positioned anywhere between its min_height and height. Nothing special here, this just means the layer is allowed to move freely within its scrollable range (from being completely visible to completely off screen.)

When there’s an animation caused by a height change, the offsets from the renderer are calculated based on the new height. In certain cases, the constraints must allow for the layer to move past its scrollable range. (For example, consider when the top toolbar is fully visible and a height decrease is being animated. The new height is smaller than the old height, so throughout the animation, the toolbar is positioned past its fully visible position.) Constraints are [updated](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;l=497;drc=2d6c94396f33fe6c4a95c11db6c48f45302a4ab2;bpv=0;bpt=1) when height changes (this includes when the toolbar [changes its position](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/browser_controls/android/java/src/org/chromium/chrome/browser/browser_controls/BrowserControlsSizer.java;l=74;drc=2d6c94396f33fe6c4a95c11db6c48f45302a4ab2;bpv=1;bpt=1).)

### Logic after BCIV

These assumptions must hold before browser controls become scrollable/animatable:

- There is always a period of time where the BrowserControlsState is kShown or kHidden.
- Viz has already received a browser frame with the capture of the latest UI state.

#### Stack trace for scrolling

1. [Visibility constraint changes] [TabBrowserControlsConstraintsHelper](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/tab/TabBrowserControlsConstraintsHelper.java;l=274?q=generateoffsettag&ss=chromium%2Fchromium%2Fsrc) gets notified that the constraint has changed to kBoth. This indicates that the controls are now scrollable.
2. [Generate new OffsetTags] Create a [BrowserControlsOffsetTagsInfo](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/browser_controls/android/java/src/org/chromium/chrome/browser/browser_controls/BrowserControlsOffsetTagsInfo.java;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1;l=20) (container class that bundles together OffsetTag related objects to make it more convenient to pass around in java) and supplies it with newly [generated](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/tab/TabBrowserControlsConstraintsHelper.java;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=1;bpt=1;l=242?q=generateoffsettag&ss=chromium%2Fchromium%2Fsrc) OffsetTags.
3. [Tag cc::Layers with new OffsetTags] [BrowserControlsManager](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;l=300;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1) gets notified of the new tags and supplies BrowserControlsOffsetTagsInfo with:
    - mTopControlsAdditionalHeight and mBottomControlsAdditionalHeight (for making the shadow disappear.)
    - OffsetTagConstraints for the top controls and content layer.
It also [notifies](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1;l=972) its own observers of the change:
    - [StaticLayout](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/compositor/layouts/StaticLayout.java;l=170;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8), to tag the content layer with the new content OffsetTag.
    - [TopToolbarOverlayMediator](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/top/TopToolbarOverlayMediator.java;l=231;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8), to tag the toolbar with the appropriate OffsetTag depending on where it’s positioned (either top or bottom.)
    - [BottomControlsStacker](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/browser_controls/android/java/src/org/chromium/chrome/browser/browser_controls/BottomControlsStacker.java;l=370;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1), to tag the bottom controls (currently consists of [tabgroup strip](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/toolbar/java/src/org/chromium/chrome/browser/toolbar/bottom/BottomControlsMediator.java;l=356;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8) and [bottom chin](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/edge_to_edge/internal/java/src/org/chromium/chrome/browser/ui/edge_to_edge/EdgeToEdgeBottomChinMediator.java;l=321;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1)) with the new bottom controls OffsetTag. Also updates mBottomControlsAdditionalHeight in BrowserControlsManager so that it can be supplied to BrowserControlsOffsetTagsInfo.
4. [Pass OffsetTags to DelegatedFrameHostAndroid] BrowserControlsManager creates a [BrowserControlsOffsetTagDefinitions](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/BrowserControlsOffsetTagDefinitions.java;l=20;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=0) (container class that bundles together tags and constraints, which are the only things the frame host needs) and passes it via [updateOffsetTagDefinitions](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;l=1244;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=1;bpt=1). This eventually calls [registerOffsetTags](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/delegated_frame_host_android.cc;l=132;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1). The tag definitions and will get passed to viz as part of the [metadata](https://source.chromium.org/chromium/chromium/src/+/main:cc/slim/layer_tree_impl.cc;l=497;drc=2d6c94396f33fe6c4a95c11db6c48f45302a4ab2) when the browser compositor frame is submitted.
5. [Pass OffsetTags to renderer] TabBrowserControlsConstraintsHelper creates a [BrowserControlsOffsetTagModifications](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/android/java/src/org/chromium/cc/input/BrowserControlsOffsetTagModifications.java;l=16;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8) (container class that bundles together tags and top+bottom additional heights, which are the only things the renderer needs) and passes it via [updateState](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/tab/TabBrowserControlsConstraintsHelper.java;l=305), which eventually calls [BrowserControlsOffsetManager::UpdateBrowserControlsState](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/browser_controls_offset_manager.cc;l=190?q=UpdateBrowserControlsState%20f:cc&ss=chromium%2Fchromium%2Fsrc).
6. [Initiating a scroll] Browser main thread still handles the touch events, and dispatches them to the renderer.
7. [Create OffsetTagValues] The renderer’s LayerTreeHostImpl calculates the offset of the browser controls for the next frame in the scroll, applying the additional offset for the top/bottom shadow if needed. The offset is then converted into an OffsetTagValue and will get passed to viz as part of the [metadata](https://source.chromium.org/chromium/chromium/src/+/main:cc/trees/layer_tree_host_impl.cc;l=2561?q=additionalheight%20f:layer_tree%20&ss=chromium%2Fchromium%2Fsrc) when the renderer compositor frame is submitted.
8. [Notify browser of offset change] The renderer notifies the browser via [onControlsOffsetChanged](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;l=936;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1). Without BCIV, each observer would set their property models’ y-offset to height+offset. This is what incurs a browser frame and results in jank. With BCIV, we always set the y-offset to the height. This is a no-op when scrolling, but is necessary for animations. Since the property model didn’t change, the browser won’t submit a new frame.
9. [Producing a frame] Viz will try to (modulo any scheduling issues) produce a frame as soon as the renderer frame from step 7 is received. Viz aggregates this renderer frame with the browser frame it received before the scroll began. The browser frame contains the browser controls, the renderer frame contains everything else. The frames get decomposed into quads, some of which will be tagged. For all tagged quads, viz looks for an OffsetTagValue with a matching tag, and will apply the appropriate translation on the quad.

#### Stack trace for animations

1. [Generate and distribute OffsetTag and other objects] Steps 1-5 for scrolling with BCIV.
6. [Initiating an animation] All animations are initiated by one of the following:
    - A change in the visibility constraints that forces the controls to be shown or hidden (for example, the controls are scrolled offscreen, but starting a navigation would force the controls to be fully visible.)
    - A height and/or min height [change](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;l=520;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1) (for example, losing wifi and having the status indicator increase the height and min height by pushing down the top toolbar.)
7. [Adjusting OffsetTagConstraints] For an animation caused by visibility constraints changing, nothing needs to be done here, the controls don’t need to move outside its scrollable range to satisfy the animation. For animations caused by height changes, the constraints must be [adjusted](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1;l=497), as they need to move outside their scrollable range.
8. [Renderer creates the Animation] BrowserControlsOffsetManager is responsible for handling all animations. Height change animations are created [here](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/browser_controls_offset_manager.cc;l=746;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=0;bpt=1).
9. [Animation tick] The animation [ticks](https://source.chromium.org/chromium/chromium/src/+/main:cc/input/browser_controls_offset_manager.cc;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=1;bpt=1;l=842) until it is completed. During each tick, the renderer calculates the new offset and converts it to an OffsetTagValue (same as step 7 for scrolling) and notifies the browser of the offset change (same as step 8 for scrolling.) It is important to note that the offset is calculated based on the final height of the layer, so viz must apply the offset on a frame with the controls positioned at the new height. For this to happen, the browser must update its property models with the new height and submit a new frame (SurfaceSync will be applied here, because the OffsetTagValues from the renderer's frame must be applied on this frame). Incurring an extra browser frame here is ok for now (we aren’t measuring how janky animations are, and this is still a net improvement.) The call to onBrowserControlsOffsetChanged after the first tick must be the exact time the property models are set with the new height. Otherwise, one of the following occurs and the animation will flicker/jump:
- Property models are updated too early, causing viz to receive the browser frame before the renderer frame, resulting in drawing a frame with the correct height but old offset.
    - Property models are updated too late, causing viz will receive the renderer frame before the browser frame, resulting in drawing a frame with the correct offset, but old height.
10. [Animation completes] After the final onBrowserControlsOffsetChanged, we [update](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;l=508;drc=39ba0aa0c66c5d257ff955964f8c10d654f964e8;bpv=1;bpt=1) the constraints again to confine the controls within their scrollable range.

### Adding a new browser control for BCIV

Currently, only the top+bottom controls and content layer are moved by BCIV because they are usually always present during a scroll/animation. If there is a new/existing browser control that appears frequently and needs to be scrolled/animated, it should be moved with BCIV.

1. Determine if any existing OffsetTags can be used. If not, create a new tag and add it to BrowserControlsOffsetTags (would probably involve some refactoring of other relevant OffsetTag objects.) If a new tag is added:
    - Make sure it is properly propagated and registered everywhere. Follow the stacks for scrolling and animations to check where tags are used, and make sure the new tag is accounted for in those locations.
    - Update the renderer to correctly compute the OffsetTagValue.
2. Make sure the browser updates its property model with the height. This ensures that animations will function correctly, and not introduce unnecessary browser frames.
3. Test that the new browser control works with existing browser controls implemented with BCIV. In particular, check that:
    - There are no unnecessary browser frames being produced during a scroll. The easiest way is probably to add a log [here](https://source.chromium.org/chromium/chromium/src/+/main:cc/slim/frame_sink_impl.cc;l=364?q=frame_sink_impl&ss=chromium%2Fchromium%2Fsrc) and scroll a webpage while monitoring logcat. It is normal to see a frame in these cases, but there should be zero frames at all times during the scroll.
        - When the visibility constraints change and controls become scrollable.
        - A few seconds after the controls become scrollable (from delayed tasks updating the UI)
        - At the end of a scroll (after lifting up the finger/tool used for the scroll)
    - There are no visible jumping/flickering during scrolls and animations. If you see jumping/flickering, this usually means that there are short windows of time where the offset is incorrect. Some common animations include:
        - Hiding the browser controls and then initiating a navigation. This should animate the controls back to their fully visible state.
        - Turning off/on wifi, which animates in/out a StatusIndicator on the top of the screen. The top controls should be animated to move along with the indicator.
        - Changing the position of the toolbar. The toolbar should hide instantly, and animate to its fully visible state.
        - Entering and exiting fullscreen works. Easiest way is probably to navigate to permission.site and tapping "Fullscreen".
    - If your new browser control is stackable with other controls, check all of the above when the new control is by itself and when stacked with other controls.

Here are some example CLs for the bottom tabgroup strip that [adds and distributes OffsetTag related objects](https://chromium-review.googlesource.com/c/chromium/src/+/6001144), and [makes it move with BCIV and prevent other sources from unnecessarily submitting frames](https://chromium-review.googlesource.com/c/chromium/src/+/6018885).

## Browser controls when there is no web contents

When there is no web contents the browser controls can/should interact with, the browser controls will always be fully shown. This is currently the case for native pages such as NTP and other surfaces such as the tab switcher. However, the browser controls heights can still change, even without web contents. In this case, the transition animations are run by `BrowserControlsManager` in the browser process. See [BrowserControlsManager.java](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/fullscreen/BrowserControlsManager.java;drc=5b029d1b98e2b7b35fff59b50a54af5d47689de9;l=725). `BrowserControlsManager` animates the height changes by updating the controls offsets and the content offset over time. For every frame, `BrowserControlsStateProvider#Observer`s are notified through `#onControlsOffsetChanged()`.

## Making sure your surface works in harmony with the browser controls

- Surfaces where the browser controls are visible should never use a dimen resource (a common example is R.dimen.toolbar_height_no_shadow) to position its contents.
- If a surface needs to pad, margin or offset its contents, using the `BrowserControlsStateProvider` interface will be the best way most of the time.
- Surfaces should also be aware that the controls heights or the content offsets can change at any time. The `BrowserControlsStateProvider#Observer` interface can be used for this purpose. Also, a height change is very likely to be followed by content/controls offset updates over time. In this case, basing any margin/padding/offset updates on `Observer#onTopControlsHeightChanged` calls might cause a jump instead of a smooth animation.
