See [Testing and infrastructure](https://sites.google.com/a/chromium.org/dev/developers/testing) for more information.

| Type of test           | Description |
|:-----------------------|:------------|
|accessibility\_unittests| |
|angle\_unittests        | |
|app\_list\_unittests    | |
|ash\_unittests          | |
|aura\_unittests         | |
|base\_i18n\_perftests   | |
|base\_perftests         |Performance tests for base module.|
|base\_unittests         |Tests the base module.|
|blink\_heap\_unittests  | |
|blink\_platform\_unittests| |
|breakpad\_unittests     | |
|[browser\_tests](https://sites.google.com/a/chromium.org/dev/developers/testing/browser-tests)|Tests the browser UI. Can not inject user input or depend on focus/activation behavior because it can be run in parallel processes and/or with a locked screen, headless etc. For tests sensitive to that, use interactive\_ui\_tests. For example, when tests need to navigate to chrome://hang (see chrome/browser/ui/webui/ntp/new\_tab\_ui\_uitest.cc)|
|chromedriver\_unittests | |
|content\_browsertests   |Similar to browser\_tests, but with a minimal shell contained entirely within content/. This test, as well as the entire content module, has no dependencies on chrome/.|
|content\_gl\_tests      | |
|content\_perftests      | |
|content\_unittests      | |
|courgette\_unittests    | |
|crypto\_unittests       | |
|curvecp\_unittests      | |
|device\_unittests       |Tests for the device (Bluetooth, HID, USB, etc.) APIs.|
|ffmpeg\_tests           | |
|ffmpeg\_unittests       | |
|gfx\_unittests          | |
|gpu\_tests              | |
|interactive\_ui\_tests  |Like browser\_tests, but these tests do things like changing window focus, so that the machine running the test can't be used while the test is running. May include browsertests (derived from InProcessBrowserTest) to run in-process in case when the test is sensitive to focus transitions or injects user input/mouse events.|
|ipc\_tests              |Tests the IPC subsystem for communication between browser, renderer, and plugin processes.|
|jingle\_unittests       | |
|media\_unittests        | |
|memory\_test            | |
|net\_perftests          |Performance tests for the disk cache and cookie storage.|
|net\_unittests          |Unit tests network stack.|
|[page\_cycler\_tests](https://sites.google.com/a/chromium.org/dev/developers/testing/page-cyclers)| |
|performance\_ui\_tests  | |
|plugin\_tests           |Tests the plugin subsystem.|
|ppapi\_unittests        |Tests to verify Chromium recovery after hanging or crashing of renderers.|
|printing\_unittests     | |
|reliability\_tests      | |
|safe\_browsing\_tests   | |
|sql\_unittests          | |
|startup\_tests          |Test startup performance of Chromium.|
|sync\_integration\_tests| |
|sync\_unit\_tests       | |
|tab\_switching\_test    |Test tab switching functionality.|
|telemetry\_unittests    |Tests for the core functionality of the Telemetry performance testing framework. Not performance-sensitive.|
|telemetry\_perf\_unittests|Smoke tests to catch errors running performance tests before they run on the chromium.perf waterfall. Not performance-sensitive.|
|test\_shell\_tests      |A collection of tests within the Test Shell.|
|[test\_installer](https://sites.google.com/a/chromium.org/dev/developers/testing/windows-installer-tests)|Tests Chrome's installer for Windows|
|ui\_base\_unittests     |Unit tests for //ui/base.|
|unit\_tests             |The kitchen sink for unit tests. These tests cover several modules within Chromium.|
|url\_unittests          | |
|views\_unittests        | |
|wav\_ola\_test          | |
|webkit\_unit\_tests     | |
|webui tests             | Special type of browser\_tests used for [WebUI features](https://chromium.googlesource.com/chromium/src/+/main/docs/webui_explainer.md), see [here](https://docs.google.com/document/d/1Z18WTNv28z5FW3smNEm_GtsfVD2IL-CmmAikwjw3ryo/edit#) for more information on known issues with WebUI test infrastructure. |
