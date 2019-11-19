# Night Mode (aka Dark Theme) on Chrome Android

Night mode (aka Dark Theme) enables users to experience UI surfaces, rendered as Android views, in dark colors. All existing or new user-facing features on Chrome Android should implement a night mode variant of the relevant UI surfaces on Milestone M75+.

[TOC]

## Implement night mode UI for new features
###Colors
Colors defined in **color_palette.xml** are independent of night mode (i.e. will not change in night mode), and are used for color references defined in **values/colors.xml** and **values-night/colors.xml**.

Color references in values/colors.xml will be used for day mode (aka light theme), and also for night mode if the particular color reference is not defined in values-night/colors.xml. Color references in values-night/colors.xml will be used for night mode.

**Example**  
In most cases, you should make use of the color references that are already defined in [//src/ui/android/java/res/values/color_palette.xml](https://cs.chromium.org/chromium/src/ui/android/java/res/values/color_palette.xml?q=color_palette.xml&sq=package:chromium&dr) and [//src/ui/android/java/res/values/colors.xml](https://cs.chromium.org/chromium/src/ui/android/java/res/values/colors.xml?sq=package:chromium&dr&g=0) for your new feature.

However as an example, suppose you got approval from snowflake-team@chromium.org to add a new background color.

In [//src/ui/android/java/res/values/color_palette.xml](https://cs.chromium.org/chromium/src/ui/android/java/res/values/color_palette.xml?q=color_palette.xml&sq=package:chromium&dr), add
```xml
<color name="new_bg_color_light">some light background color</color>
<color name="new_bg_color_dark">some dark background color</color>
```

In [//src/ui/android/java/res/values/colors.xml](https://cs.chromium.org/chromium/src/ui/android/java/res/values/colors.xml?sq=package:chromium&dr&g=0), add
```xml
<color name="new_bg_color">@color/new_bg_color_light</color>
```

In [//src/ui/android/java/res_night/values-night/colors.xml](https://cs.chromium.org/chromium/src/ui/android/java/res_night/values-night/colors.xml), add
```xml
<color name="new_bg_color">@color/new_bg_color_dark</color>
```

An example to use this color in XML:
```xml
<View
	...
	android:background="new_bg_color" />
```

An example to use this color in Java:
```java
mView.setBackgroundResource(R.color.new_bg_color);
mView.setBackgroundColor(
    ApiCompatibilityUtils.getColor(mView.getResources(), R.color.new_bg_color));
```

Optionally, if the color is used exclusively for your feature, or if you want to easily update this color for your feature in the future, in the values/colors.xml that contains colors specifically for your feature, add
```xml
<color name="my_shiny_new_feature_bg_color">@color/new_bg_color</color>
```

If your feature needs colors that don't change based on day/night mode (e.g incognito mode UI), in the values/colors.xml that contains colors specifically for your feature, reference the colors defined in color_palette.xml. There is no need to define the color reference in values-night/colors.xml.
```xml
<!-- Dark background color for my shiny new feature regardless of day/night mode. -->
<color name="my_shiny_new_feature_bg_color_dark">@color/new_bg_color_dark</color>
```

###Styles
Colors used in styles can be either adaptcive or independent of night mode. When using existing or adding new styles, make sure the colors used in the styles fit your need.

**Best practice of naming styles**

* If the color adapts for night mode, avoid mentioning a specific color in the style name since it may not be accurate in night mode.
```xml
<!-- OK -->
<style name="TextAppearance.Headline">
  <!-- default_text_color is dark grey in day mode, and white in night mode. -->
  <item name="android:textColor">@color/default_text_color</item>
  ...
</style>

<!-- NOT OK -->
<style name="TextAppearance.DarkGreyHeadline">
  <!-- default_text_color is dark grey in day mode, and white in night mode. -->
  <item name="android:textColor">@color/default_text_color</item>
  ...
</style>

<!-- OK -->
<style name="TextAppearance.ButtonText.Blue">
  <!-- some_blue_color is dark blue in day mode, and light blue in night mode. -->
  <item name="android:textColor">@color/some_blue_color</item>
</style>
```
* If independent of night mode, mention a specific color or where it is generally used.
```xml
<!-- OK -->
<style name="TextAppearance.Headline.White">
  <item name="android:textColor">@android:color/white</item>
  ...
</style>

<!-- OK -->
<style name="TextAppearance.Body.Incognito">
  <item name="android:textColor">@android:color/white</item>
  ...
</style>
```

###Themes
If adding a new theme, make sure the parent (or any indirect ancestor) theme of the new theme is one of the AppCompat DayNight themes (prefixed with `Theme.AppCompat.DayNight`), or alternatively, define the same theme in values-night/ with the desired parent theme for night mode. See [dark theme](https://developer.android.com/preview/features/darktheme) in Android developer guide for more details.

###Troubleshooting
* Make sure `View` is inflated from `Activity` context instead of `Application` context
  * `RemoteView` is an exception. See [RemoteViewsWithNightModeInflater.java](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/night_mode/RemoteViewsWithNightModeInflater.java?) for details.
* Make sure color resources are accessed from `Activity` or `View` context instead of `Application` context
* Check whether `Configuration.uiMode & UI_MODE_NIGHT_MASK` gives the correct UI night mode
  * If uiMode is not correct, it could be a support library issue or an Android framework issue. You can contact chrome-android-app@chromium.org for help.

## Test new features in night mode
### Automatic Testing
Render tests are the recommended way to verify the appearance of night mode UI. If you are not familiar with render tests, please take a look at [render test instructions](https://github.com/endlessm/chromium-browser/blob/master/chrome/test/android/javatests/src/org/chromium/chrome/test/util/RENDER_TESTS.md) to learn about how to write a new render test and upload golden images.

**For tests using DummyUiActivity:**

* Put all the render tests into a separate test suite
* Use class parameter [`NightModeTestUtils.NightModeParams.class`](https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/night_mode/NightModeTestUtils.java?type=cs&q=NightModeTestUtils.NightModeParams)
* Pass in a boolean parameter that indicates night mode state in constructor
* Set up night mode in constructor by calling [`NightModeTestUtils#setUpNightModeForDummyUiActivity()`](https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/night_mode/NightModeTestUtils.java?type=cs&q=setUpNightModeForDummyUiActivity&sq=package:chromium) and [`RenderTestRule#setNightModeEnabled()`](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/util/RenderTestRule.java?type=cs&q=setNightModeEnabled)
* During [`tearDownTest()`](https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/test/ui/DummyUiActivityTestCase.java?type=cs&q=tearDownTest), reset night mode state by calling [`NightModeTestUtils#tearDownNightModeForDummyUiActivity()`](https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/night_mode/NightModeTestUtils.java?type=cs&q=tearDownNightModeForDummyUiActivity)

See [this CL](https://chromium-review.googlesource.com/c/chromium/src/+/1613883) as an example

**For tests using ChromeActivityTestRule:**

* In the method annotated with `@BeforeClass`, initialize states by calling [`NightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched()`](https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/night_mode/NightModeTestUtils.java?type=cs&q=setUpNightModeBeforeChromeActivityLaunched)
* Add method `setupNightMode()` with annotation `@ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)`
* In method `setupNightMode()`, set up night mode state by calling [`NightModeTestUtils#setUpNightModeForChromeActivity()`](https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/night_mode/NightModeTestUtils.java?type=cs&q=setUpNightModeForChromeActivity) and [`RenderTestRule#setNightModeEnabled()`](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/util/RenderTestRule.java?type=cs&q=setNightModeEnabled)
* In the method annotated with `@AfterClass`, reset night mode state by calling [`tearDownNightModeAfterChromeActivityDestroyed`](https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/night_mode/NightModeTestUtils.java?type=cs&q=tearDownNightModeAfterChromeActivityDestroyed)

See [this CL](https://chromium-review.googlesource.com/c/chromium/src/+/1656668) as an example

###Manual Testing
Different ways to turn on night mode:

* Go to Chrome **Settings -> Themes** on Android L+
* Turn on power save mode (aka **battery saver**) on Android L+
* Go to **Android Settings -> Developer options -> Night mode** on Android P
* Go to **Android Settings -> Display -> Theme** on Android Q

Ways to turn on night mode on **custom tab**:

* Turn on power save mode (aka **battery saver**) on Android P+
* Go to **Android Settings -> Developer options -> Night mode** on Android P
* Go to **Android Settings -> Display -> Theme** on Android Q
* [Set color scheme](https://cs.chromium.org/chromium/src/third_party/android_sdk/androidx_browser/browser/src/main/java/androidx/browser/customtabs/CustomTabsIntent.java?) to `COLOR_SCHEME_DARK` on creating a `CustomTabsIntent.Builder`

Some tips:

* If building **chrome\_apk**, add `compress_resources = false` in gn args to disable Lemon compression. See [Issue 957286](https://crbug.com/957286) for details.
* Night mode is only available on L+
* Animation is turned off when in power save mode on Andoird L-O

## Optional: Add independent night mode control to an Activity
Most of the features will follow the app-wise night mode control, but some features might require introduction of an independent night mode control. For example, custom tab will not follow the app-wise night mode control, but instead, will respect the night mode settings from the host app. In such cases, you can

1. Create your own implementation of [`NightModeStateProvider`](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/night_mode/NightModeStateProvider.java)
2. Override [`ChromeBaseAppCompatActivity#createNightModeStateProvier()`](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/ChromeBaseAppCompatActivity.java?type=cs&q=createNightModeStateProvider)
3. Use [`AppCompatDelegate#setLocalNightMode()`](https://developer.android.com/reference/android/support/v7/app/AppCompatDelegate.html#setLocalNightMode(int)) to update night mode in the `Configuration` of the `Activity`
