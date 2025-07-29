Most features built for Chrome for Android are applicable across all form factors (phones, tablets, experimental desktop, etc). While some features may be used more on certain form factors, that they are typically still useful across the breadth of devices Chrome for Android supports. We also generally strive to provide consistent feature experience across all form factors and minimize divergence; rather, features should be available and UI layouts or entry points should adapt to the screen size.

Sometimes, however, there is a need to make features conditionally available. The criteria for when a feature should be available will vary and should be established in the PRD phase. Below is a list of options and some examples of when they may be applicable. Note that some features may use more than one of these signals to determine availability.

**Priority #1 - Most reliable and least ambiguous criterion to check. Directly answers the question: "Does the device have the specific technical ability to support this feature?"**
* **Purpose built Android APIs:** where possible, it's preferred to detect the specific device capabilities that are needed to support a feature. Examples:
  * Query for capabilities using PackageManager#hasSystemFeature, e.g. FEATURE_PICTURE_IN_PICTURE which is true on most form factors, but false on automotive currently.
* **Input modality:** touch, mouse, stylus, keyboard
  * Touch provides a more limited range of interactions (tap or long press) compared to mouse (click, right click, hover).
  * Guidance for input modality checks:
    * Use **MotionEvent#getToolType(int)** to get the tool type during a motion event.
    * Use **[DeviceInput](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/base/DeviceInput.java)t#supportsKeyboard() / supportsPrecisionPointer()** to check for presence of mouse and keyboard. DeviceInput also listens to input update events (attached / detached). Caveats:
      * supportsPrecisionPointer() returns true for Samsung devices without peripherals attached. Tracking bug: crbug.com/429262357
      * supportsPrecisionPointer() does not detect stylus. Stylus attached can be detected during onMotionEvent()

**Priority #2 - This addresses the practical usability of a feature within its current presentation layer**
* **Screen size:** screen real estate can be a limiting factor, particularly for showing top-level UI.
  * Guidance for screen size vs window size checks:
    * **WindowMetrics#getBounds()** - bounds of app's window in screen coordinates (accounts for multi-window mode). Use this API to get window bounds.
    * **[DisplayUtil](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/display/DisplayUtil.java)#getCurrentSmallestScreenWidth()** - Uses Android's WindowManager#getMaximumWindowMetrics() to determine display width associated with the context provided.
    * **Configuration#screenWidthDp or screenHeightDp** - For Android V+, it is the same as WindowMetrics. Otherwise, differs from WindowMetrics by not including window insets in measurement. Rounded to the nearest dp rather than px. In multiple-screen scenarios, the width measurement can span screens.
      * Note: Configuration#smallestScreenWidthDp is overridden to display width for all activities that extend [ChromeBaseAppCompatActivity](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/ChromeBaseAppCompatActivity.java)
* **Windowing mode:**
  * Split screen or floating windows: Android's Activity#isInMultiWindowMode returns true for split screen and free form window
  * Desktop windowing mode: no API, not recommended for conditionally enabling features.
  * Guidance for windowing mode checks:
    * Use **[MultiWindowUtil](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/multiwindow/MultiWindowUtils.java)#isInMultiWindowMode()** to check if activity is in one of above modes.
      * Use **[AppHeaderUtils](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/desktop_windowing/java/src/org/chromium/chrome/browser/ui/desktop_windowing/AppHeaderUtils.java)#isAppInDesktopWindow()** to check if app shows tab strip along with sysUI buttons
    * Multi-display or Connected displays: These are secondary displays where the primary display is an Android device. **DisplayAndroidManager** has util methods to get display based on activity context. Chrome currently re-launches the ChromeActivity on display density change (eg: app moved across displays)

**Priority #3 - Safeguard/proxy to limit memory-intensive features to "higher end devices", but hard to explain to users**
* **RAM:** sometimes features are more memory intensive or come with other performance costs. Limiting those features to higher end devices, using RAM as a proxy, can provide the right balance between bringing premium features to more premium devices without regressing core experiences on lower end devices.

**Priority #4** - This is because "form factor" is a blunt and often misleading proxy for what a feature actually needs (e.g., screen size, input modality, or a specific API). For example, a "tablet" could have a small screen and no keyboard, while a "phone" could be connected to a large monitor with a mouse. Also, limits cross-device continuity.
* **Form factor (e.g. phone vs tablet)**
  * **Phones vs Large form factors** - use [DeviceFormFactor](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/base/DeviceFormFactor.java) isNonMultiDisplayContextOnTablet() or isWindowOnTablet() methods to differentiate devices based on display size (threshold = 600dp). Large form factor devices include tablets, some automotives, XR, experimental Desktop and Phones with Chrome running on a connected display.
  * **Foldables** - The inner screen of foldable is treated as a large form factor. Chrome currently re-launches the ChromeActivity on fold transition and DeviceFormFactor can be used to differentiate outer vs inner layout. However, we are actively moving away to avoid relaunch and the best practice is to make layouts adaptive to window size changes. Foldables in general can be differentiated via DeviceInfo.isFoldable().
  * **Android Automotive, XR and experimental Desktop** - Use [DeviceInfo](https://source.chromium.org/chromium/chromium/src/+/main:base/android/java/src/org/chromium/base/DeviceInfo.java) methods for specific device type detection.

There are some conditions that are generally discouraged:
* **OEMs:** checking for a specific OEM tends not to be scalable or maintainable over time. At times, teams may choose to enable a feature for a particular OEM or set of devices during dogfooding, however, that criteria should usually be removed prior to a full launch.

#### Including feature in build per form factor
If a feature could significantly impact binary size and needs to be enabled for a specific form factor, utilize ServiceLoaderUtil to instantiate your classes and conditionally enable these in your BUILD.gn files.
#### Simulations:
* Simulate Desktop:
  * Set IS_DESKTOP_ANDROID in gn args to include desktop features in build.
  * Set --force-desktop-android as command line arg when launching the app to override runtime desktop check in DeviceInfo
* Simulate LFF (display size >= 600dp)
  * Set “Smallest width” in Developer options to 600 or above to simulate large screen layout on phones.
#### Automation Tests:
Current mechanisms:
* Use DeviceFormFactor to restrict or disable features for phones / tablets / desktop for instrumentation tests. Use Roboelectric @Config with width qualifiers to simulate phone vs large form factor in unit tests.
* Use [OverrideContextWrapperTestRule](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/android/javatests/src/org/chromium/chrome/test/OverrideContextWrapperTestRule.java) to override DeviceInfo values for desktop and automotive for unit tests.