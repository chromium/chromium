# How to Develop With Dynamic Colors

## Background

[Dynamic Color](https://m3.material.io/styles/color/dynamic-color) is a new feature introduced as part of Android S (aka Android 12/API 31) and Material You in which the applications can be themed using a custom color palette extracted from the user’s wallpaper.

## How does it work?

The Material 3 theme contains color attributes that correspond to the material “color roles.” When we call [`DynamicColors#applyIfAvailable()`](https://github.com/material-components/material-components-android/blob/b70bbc2942bdbd1ea763e72a6b1e561e4813f10c/lib/java/com/google/android/material/color/DynamicColors.java#L211) in [`ChromeBaseAppCompatActivity#onCreate`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/ChromeBaseAppCompatActivity.java;drc=4d2b5adb556128aab9313fc3851f192c254e09cb;l=218) (or any other activity’s `#onCreate` method), the color role attributes are overridden using colors that are extracted by the system from the user’s wallpaper. Finally, any UI surface that references the color role attributes gets dynamically colored. The color role attributes can be found [here](https://m3.material.io/libraries/mdc-android/color-theming).

Initially the Activity Theme sets up a mapping of "baseline" colors so that these attributes work on devices before Android S. Once the `DynamicColors#apply...` call is made, this change is applied at runtime. Using one of the color attributes, `colorPrimary`, as an example:
```
?attr/colorPrimary -> @color/baseline_primary_600 -> #0B57D0
```
becomes
```
?attr/colorPrimary -> @android:color/system_accent1_600 -> #616200
```
once the dynamic colors are applied.


## How to…

Basically, the app UI surfaces need to directly or indirectly reference the color role attributes rather than color resources. There are different ways to achieve this depending on the situation.


### Semantic names

Semantic names are used to color the UI components that share the same meaning or role consistently throughout the application. For example, `default_icon_color` can be used almost anywhere to tint a primary icon. A list of common semantic names can be found in [semantic_colors_dynamic.xml](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/styles/android/java/res/values/semantic_colors_dynamic.xml;drc=c83636b34a3e3751c28b3e43af616226f5ea111c). Before we needed to support dynamic colors, semantic names were defined as `@color` resources that could reference other colors. Now that the colors need to reference attributes, it is not possible to continue using `@color`s because Android does not support referencing an `?attr` from a `@color`, so now we use `@macro`s or color state lists.

UX mocks should contains semantic color names, and should be the level of detail eng needs to start implementation. Sometimes the semantic name may not exist in the code base yet, in which case it'll typically need to be mapped to both an adaptive baseline value and a color role attribute. Googlers can view [go/mapping-dynamic-colors-clank-mocks](go/mapping-dynamic-colors-clank-mocks) for more detailed steps of mapping colors from Figma mocks.

#### Macros

In xml, semantic names for dynamic colors are defined using `<macro>` tags. A macro is replaced with the value it holds at the build time, similar to the C++ macros. Googlers can learn more about macros [here](http://go/aapt2-macro). Macros can be used in xml to color views and drawables similarly to the color resources (`@color/`) or theme attributes (`?attr/`). Unlike colors, macros are not resources. So, macros cannot be declared in non-default configurations, e.g. `values-night`.


##### Utility methods

At the time of writing, there is no support for macros in Java code. So, we have created utility classes with static methods to access the semantic names in code. For example, if a semantic name is defined as [`@macro/default_bg_color`](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/styles/android/java/res/values/semantic_colors_dynamic.xml;drc=efb53ff2cb5ea3db8643840d7a9bde4ecdab1741;l=7) in xml, it would also have a utility method [`SemanticColorUtils#getDefaultBgColor()`](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/styles/android/java/src/org/chromium/components/browser_ui/styles/SemanticColorUtils.java;drc=fefb943cdc56d80fda8a2e13fc9327e91567e5bc;l=45) in Java.


##### Location

While the most common semantic names are defined in [`semantic_colors_dynamic.xml`](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/styles/android/java/res/values/semantic_colors_dynamic.xml;drc=efb53ff2cb5ea3db8643840d7a9bde4ecdab1741), feature or surface specific semantic names can be defined in their relative modules or directories. Then, they can reference other macros or directly reference theme attributes. For example, [`suggestion_url_color`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/res/values/colors.xml;drc=efb53ff2cb5ea3db8643840d7a9bde4ecdab1741;l=10) is defined in `chrome/browser/ui/android/omnibox/` and it references `@macro/default_text_color_link`, a common semantic name. If this semantic color needs to be accessed from Java code, a utility method can be added to the utility class for that directory, e.g. [ChromeSemanticColorUtils](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/theme/java/src/org/chromium/chrome/browser/ui/theme/ChromeSemanticColorUtils.java).


#### Color state lists

Color state lists are defined using the `<selector>` tag and are usually used to update the visual representation of the views based on their state, e.g. enabled vs disabled. Unlike the regular color resources, color state lists can reference attributes (and macros). Color state lists can be used to color surfaces as long as they point to dynamic colors, e.g. [`default_text_color_accent1_tint_list`](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/styles/android/java/res/color/default_text_color_accent1_tint_list.xml). Keep in mind that there may be limitations to the color state list usage. For example, color state lists cannot be used as `android:background` below API 29.


### Surface colors

[Surface colors](https://m3.material.io/styles/color/the-color-system/color-roles#c0cdc1ba-7e67-4d6a-b294-218f659ff648) represent `?attr/colorSurface` at different surface levels or elevation values. With the exception of Surface-0 (just route through `?attr/colorSurface`), the rest of the surface colors must be calculated at runtime. This means there is no macro or attribute that can be used to retrieve surface colors. For this reason, there are currently 2 ways to calculate surface colors.

Note: Numeric/elevation surface colors are in the process of being removed or remapped to tone-based surface colors. Minor colors shifts have been applied to Chrome, but the code is currently still using the old numeric/elevation surface color approach.

#### ChromeColors#getSurfaceColor()

[`ChromeColors#getSurfaceColor()`](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/styles/android/java/src/org/chromium/components/browser_ui/styles/ChromeColors.java;l=153;drc=8989e41e6a3db288b26ff624819d71193554b06a) calculates a surface color using the required attributes from the provided `Context` and the elevation resource. The elevation resource should be one of the [predefined elevation levels](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/res/values/dimens.xml;drc=0836f570d8e966dc6836120efa7998ac87a5d99b;l=76), or a resource that points to one of these.


#### SurfaceColorDrawable

[`SurfaceColorDrawable`](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/widget/android/java/src/org/chromium/components/browser_ui/widget/SurfaceColorDrawable.java;drc=9636025627ac8986e82cfaeb5a633c2f7d53238f;l=41) is a custom drawable that automatically calculates its surface color based on the provided `app:surfaceElevation` attribute. This can be used in xml to define a drawable similarly to `<shape>`, e.g. [`oval_surface_1`](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/widget/android/java/res/drawable-v31/oval_surface_1.xml;drc=1eeb153bf06ca256b6a132d9e17fd6a83e702bc4). Similar to the `ChromeColors#getSurfaceColor()` method, the provided `app:surfaceElevation` should be one of the predefined elevation levels (mentioned above).


### Illustrations

The guidance for illustrations is still a work in progress. Until it is finalized, the recommended approach is continuing the use of non-dynamic colors (see the non-dynamic colors section below) but making the illustration’s background transparent. In the future, we may recommend using color resources that point to system color resources.


### Launcher widgets

The guidance for widgets is not finalized, so the instructions on the [Enhance your widget developer page](https://developer.android.com/guide/topics/appwidgets/enhance#dynamic-colors) can be followed. However, the mentioned “device theming” approach is extremely limited. At some point, we may recommend using system color resources similarly to the illustrations.


## Non-dynamic colors

Not all colors can or should be dynamic. Some examples are migration-to-dynamic-colors-is-work-in-progress surfaces, incognito surfaces, and WebView. Non-dynamic colors still follow the semantic name pattern to keep colors consistent throughout Chrome. They are typically defined in [semantic_colors_adaptive.xml](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/res/values/semantic_colors_adaptive.xml;drc=0836f570d8e966dc6836120efa7998ac87a5d99b), which contains  colors that adapt to day and night modes and are suffixed with _baseline to indicate that they are not dynamic, or [semantic_colors_non_adaptive.xml](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/res/values/semantic_colors_non_adaptive.xml;drc=0836f570d8e966dc6836120efa7998ac87a5d99b) which do not adapt for day night mode and may be used for e.g. incognito coloring.


### Migration-to-dynamic-colors-is-work-in-progress

The majority of highly visible surfaces in Chrome on Android have been migrated to fully support dynamic colors. However there's also a long tail of surfaces that were not updated. If you notice anything like this on a surface you own or maintain, please look at migrating those colors to dynamic colors. Otherwise, you can file a bug using [this link](https://bugs.chromium.org/p/chromium/issues/entry?summary=Issue+Summary&comment=Application+Version+%28from+%22Chrome+Settings+%3E+About+Chrome%22%29%3A+%0DAndroid+Build+Number+%28from+%22Android+Settings+%3E+About+Phone%2FTablet%22%29%3A+%0DDevice%3A+%0D%0DSteps+to+reproduce%3A+%0D%0DObserved+behavior%3A+%0D%0DExpected+behavior%3A+%0D%0DFrequency%3A+%0D%3Cnumber+of+times+you+were+able+to+reproduce%3E+%0D%0DAdditional+comments%3A+%0D&labels=Restrict-View-Google%2COS-Android%2CPri-2%2CHotlist-MaterialNext&cc=skym%40chromium.org).


### Incognito surfaces

Incognito surfaces should not be dynamically colored. Instead, they should be colored using the night mode baseline colors. This means using colors such as `default_icon_color_light` instead of macros or attributes.


### Colors used by WebView

Dynamic colors depend on the theme attributes defined in a `Context`’s `Theme`. In the case of WebView, the Context is not controlled by Chrome but by the embedder application. We cannot be sure that the embedder application is going to provide all attributes required by our dynamic color implementation. For this reason, any UI surface that can be depended on by WebView should avoid using dynamic colors. For shared widgets like [ButtonCompat](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/widget/ButtonCompat.java;l=43), the colors can be injected through the constructor, or a custom styleable attribute can be used to manipulate the colors when the widget is used within Chrome.

Colors and widgets that are shared between WebView and Chrome are typically defined in the `ui/android` directory. If this is not the case for a color or widget, they can be moved to `components/browser_ui`.


## Getting colors and drawables

The method used to retrieve colors or drawables needs to have access to the theme, or Context, to be able to resolve the `?attrs`. These are the commonly used methods in Chrome:
* `Context#getColor(int)`: Used to get non-dynamic colors or the default color for color state lists.
* `AppCompatResources#getColorStateList(Context, int)`: Used to get color state lists.
* `AppCompatResources#getDrawable(Context, int)`: Used to get drawables.
