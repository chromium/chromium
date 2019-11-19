# Chrome for Android UI

## Ramping-up

Android has a number of [developer guides](https://developer.android.com/guide) that are helpful for getting ramped up on general Android development. We recommend reading the following as primers for developing UI:

 * [Application fundamentals](https://developer.android.com/guide/components/fundamentals)
 * [Providing resources](https://developer.android.com/guide/topics/resources/providing-resources.html)
 * [Layouts](https://developer.android.com/guide/topics/ui/declaring-layout)
 * [Introduction to activities](https://developer.android.com/guide/components/activities/intro-activities)


## Colors and text styles

Chrome for Android has a color palette defined in [//ui/android/java/res/values/color_palette.xml](/ui/android/java/res/values/color_palette.xml) and a set of reusable semantic colors defined in [//ui/android/java/res/values/colors.xml](/ui/android/java/res/values/colors.xml). The semantic colors from colors.xml should be used to ensure colors adapt properly for dark mode and can be consistently and easily updated during product-wide visual refreshes.

For more information on selecting the right color, see [Night Mode on Chrome Android](night_mode.md).

Text should be styled with a pre-defined text appearance from [//ui/android/java/res/values-v17/styles.xml](/ui/android/java/res/values-v17/styles.xml). If leading (aka line height) is needed, use [org.chromium.ui.widget.TextViewWithLeading](/ui/android/java/src/org/chromium/ui/widget/TextViewWithLeading.java) with `app:leading` set to one of the pre-defined *_leading dimensions in [//ui/android/java/res/values/dimens.xml](/ui/android/java/res/values/dimens.xml).

## Widgets

The Chromium code base contains a number of wrappers around Android classes (to smooth over bugs or save on binary size) and many UI widgets that provide Chrome-specific behavior and/or styling.

These can be found in [//chrome/browser/ui/android/widget/](/chrome/browser/ui/android/widget/), [//ui/android/](/ui/android/), and [//chrome/android/java/src/org/chromium/chrome/browser/widget/](/chrome/android/java/src/org/chromium/chrome/browser/widget/). There is an ongoing effort to consolidate all widgets in //chrome/browser/ui/android/widget/.

## MVC

UI development should follow a modified Model-View-Controller pattern. MVC base classes live in [//ui/android/java/src/org/chromium/ui/modelutil](/ui/android/java/src/org/chromium/ui/modelutil/).

The following guides introduce MVC in Chrome for Android:

 * [So, you want to do MVC...](mvc_architecture_tutorial.md)
 * [Simple Lists in MVC Land](mvc_simple_list_tutorial.md)
 * [Simple RecyclerView use in MVC](mvc_simple_recycler_view_tutorial.md)
 * [So, you want to test MVC...](mvc_testing.md)
