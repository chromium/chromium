# Chrome for Android UI

## Ramping-up

Android has a number of [developer guides](https://developer.android.com/guide) that are helpful for getting ramped up on general Android development. We recommend reading the following as primers for developing UI:

 * [Application fundamentals](https://developer.android.com/guide/components/fundamentals)
 * [Providing resources](https://developer.android.com/guide/topics/resources/providing-resources.html)
 * [Layouts](https://developer.android.com/guide/topics/ui/declaring-layout)
 * [Introduction to activities](https://developer.android.com/guide/components/activities/intro-activities)


## Colors and text styles

[Dynamic Color doc](dynamic_colors.md) should be followed when working on UI. For colors that cannot/should not be made dynamic (as mentioned in the doc), Chrome for Android has a color palette defined in [//ui/android/java/res/values/color_palette.xml](/ui/android/java/res/values/color_palette.xml) and a set of reusable semantic colors defined in [//ui/android/java/res/values/semantic_colors_adaptive.xml](/ui/android/java/res/values/semantic_colors_adaptive.xml). The semantic colors from semantic_colors_adaptive.xml should be used to ensure colors adapt properly for dark mode and can be consistently and easily updated during product-wide visual refreshes.

For more information on selecting the right color, see [Night Mode on Chrome Android](night_mode.md).

Text should be styled with a pre-defined text appearance from [//components/browser_ui/styles/android/java/res/values/styles.xml](/components/browser_ui/styles/android/java/res/values/styles.xml). If the text color cannot/should not be dynamic, pre-defined text styles from [//ui/android/java/res/values-v17/styles.xml](/ui/android/java/res/values-v17/styles.xml) can be used. If leading (aka line height) is needed, use [org.chromium.ui.widget.TextViewWithLeading](/ui/android/java/src/org/chromium/ui/widget/TextViewWithLeading.java) with `app:leading` set to one of the pre-defined *_leading dimensions in [//ui/android/java/res/values/dimens.xml](/ui/android/java/res/values/dimens.xml).

## Widgets

The Chromium code base contains a number of wrappers around Android classes (to smooth over bugs or save on binary size) and many UI widgets that provide Chrome-specific behavior and/or styling.

These can be found in [//components/browser_ui/widget/android/](/components/browser_ui/widget/android/), [//ui/android/](/ui/android/), and [//chrome/android/java/src/org/chromium/chrome/browser/widget/](/chrome/android/java/src/org/chromium/chrome/browser/widget/). There is an ongoing effort to consolidate all widgets in //components/browser_ui/widget/android.

## MVC

UI development should follow a modified Model-View-Controller pattern. MVC base classes live in [//ui/android/java/src/org/chromium/ui/modelutil](/ui/android/java/src/org/chromium/ui/modelutil/).

The following guides introduce MVC in Chrome for Android:

 * [So, you want to do MVC...](mvc_architecture_tutorial.md)
 * [Simple Lists in MVC Land](mvc_simple_list_tutorial.md)
 * [Simple RecyclerView use in MVC](mvc_simple_recycler_view_tutorial.md)
 * [So, you want to test MVC...](mvc_testing.md)

## Styles and widgets shared with WebView

Styles and widgets in //ui/android/ may be used by WebView. UI shown in WebView is inflated using the host Activity as the Context, so Chrome's custom theme attributes won't be set and android theme attributes may or may not be set. When creating new styles/widgets or modifying styles/widgets used by WebView, either avoid theme attributes or define a fallback if the theme attribute cannot be resolved.

## Handling activity results

Currently, there are 2 custom tools to catch activity results in Chrome:
[`IntentRequestTracker`](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/base/WindowAndroid.java;l=444;drc=f4b1a5f91e14941349a0319b35f71d3efb7350af)
can be called from a `WindowAndroid` to start an activity. The provided
callback will be called when the started activity returns a result. One
known limitation is that the callback is lost if the base activity is
recreated. The result will be lost if it's received after the base activity
is recreated.
[`ActivityResultTracker`](activity_result_tracker.md) using the Android
[`ActivityResultRegistry`](https://developer.android.com/training/basics/intents/result)
API is introduced to ensure that the in-flight activities' result is caught
even after base activity's recreation.
