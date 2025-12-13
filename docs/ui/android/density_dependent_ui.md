# Developing with touch-first and click-first UX in mind

## Background

Chrome for Android's UI needs to adapt to touch-first and click-first experiences. For example, text and icons may need to be scaled down for click-first experiences compared to touch-first experiences. This is achieved through a combination of different techniques, but there are some pitfalls to avoid.

## In practice

Creating "density-adaptive" UI (UI that adapts to touch-first and click-first experiences) involves using the right resources, such as styles and dimensions.

### Theme attributes

At the lowest level are dimension theme attributes. These attributes are adjusted for the current density by applying theme overlays to the Activity theme (see here in [ChromeBaseAppCompatActivity#applyThemeOverlays](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/ChromeBaseAppCompatActivity.java;drc=6b17407cab6a70f4cd2e3f836244fbca98305b44;l=569)). [//ui/android/java/res/values/attrs.xml](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/res/values/attrs.xml;drc=8e81a0abe29f80503b701434368a4dcb47abea0d;l=76) contains these attributes. For example, `?attr/minInteractTargetSize` defines the minimum touch target size for touch-first experiences, 48dp, and the minimum click target size, 36dp, for click-first experiences. Try to keep these attributes as generic as possible. If a UI surface requires a lot of special casing, it may be better to create specific `<declare-styleable>`s and handle them locally.

Any Context that is used to inflate UI components that use these attributes needs to have the attributes defined in its theme. Typically, these attributes are defined in base app themes in [//components/browser_ui/theme/android/templates/res/values/themes.xml](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/theme/android/templates/res/values/themes.xml;drc=ddc59318c9524bb0e98a3aac20a085dd57731cee;l=148).

**Warning**: Resources that are defined in [//ui/android](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/res/;drc=4860d51b6607d69ed35019d5af69a2201c99725d) can be shared with WebView, and there is no guarantee that the WebView's Context will have these attributes defined, which can lead to crashes. One way to work around this is to use a [FillInContextThemeWrapper](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/theme/FillInContextThemeWrapper.java) to provide default values for the custom attributes (see [an example here](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/modelutil/LayoutViewBuilder.java;drc=68694e72d1549da5c064c5161dbddd11e7e98fa2;l=40)).

### Text styles

There are "density-adaptive" text styles available to use in multiple locations, depending on where they are needed. See [TextAppearance.DensityAdaptive.ListMenuItem](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/res/values/styles.xml;drc=4860d51b6607d69ed35019d5af69a2201c99725d;l=423) in `//ui/android` and [TextAppearance.DensityAdaptive.TextLarge](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/styles/android/java/res/values/styles.xml;drc=ddc59318c9524bb0e98a3aac20a085dd57731cee;l=145) in `//components/browser_ui/`. Your text styles can inherit from these styles, e,g. to create text styles with different colors.

## What's next?

You can use the currently available "density-adaptive" attributes and styles to make sure your UI adapts to touch-first and click-first experiences. Also feel free to follow the given examples to create new attributes or text styles as needed.