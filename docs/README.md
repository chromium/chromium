# Chromium docs

This directory contains chromium project documentation in
[Gitiles-flavored Markdown].  It is automatically [rendered by Gitiles].

[Gitiles-flavored Markdown]: https://gerrit.googlesource.com/gitiles/+/master/Documentation/markdown.md
[rendered by Gitiles]: https://chromium.googlesource.com/chromium/src/+/master/docs/

If you add new documents, please also add a link to them in the Document Index
below.

[TOC]

## Creating Documentation

Markdown documents must follow the
[style guide](https://github.com/google/styleguide/tree/gh-pages/docguide).

### Preview local changes using [md_browser](../tools/md_browser/):

```bash
# in chromium checkout
./tools/md_browser/md_browser.py
```

This is only an estimate. The **gitiles** view may differ.

### Review changes online with gerrit's links to gitiles:

1.  Upload a patch to gerrit, or receive a review request.
    e.g. https://chromium-review.googlesource.com/c/572236
2.  View a specific .md file.
    e.g. https://chromium-review.googlesource.com/c/572236/2/docs/README.md
3.  Click on **gitiles** link at top of page.

This **gitiles** view is the authoritative view, exactly the same as will be
used when committed.

## Document Index

### Checking Out and Building
*   [Linux Build Instructions](linux_build_instructions.md) - Linux
*   [Mac Build Instructions](mac_build_instructions.md) - MacOS
*   [Windows Build Instructions](windows_build_instructions.md) - Windows
*   [Android Build Instructions](android_build_instructions.md) - Android target
    (on a Linux host)
*   [Cast Build Instructions](linux_cast_build_instructions.md) - Cast target
    (on a Linux host)
*   [Cast for Android Build Instructions](android_cast_build_instructions.md) -
    Cast for Android (on a Linux host)
*   [Fuchsia Build Instructions](fuchsia_build_instructions.md) - Fuchsia target
    (on a Linux host)
*   [iOS Build Instructions](ios/build_instructions.md) - iOS target (on a MacOS
    host)
*   [Chrome OS Build Instructions](chromeos_build_instructions.md) - Chrome OS
*   [Linux Chromium ARM Recipes](linux_chromium_arm.md) - Recipes for building
    Chromium for ARM on Linux.
*   [Chrome Component Build](component_build.md) - Faster builds using more
    libraries
*   [Using the BuildRunner](using_build_runner.md) - Scripts that extract build
    stops from builders and runs them locally on a slave
*   [Cr User Manual](cr_user_manual.md) - Manual for `cr`, a tool that tries to
    hide some of the tools used for working on Chromium behind an abstraction
    layer

### Design Docs
*   See [design/README.md](design/README.md)

### Integrated Development Environment (IDE) Set Up Guides
*   [Android Studio](android_studio.md) - Android Studio for Android builds
*   [Eclipse for Android](eclipse.md) - Eclipse for Android
*   [Eclipse for Linux](linux_eclipse_dev.md) - Eclipse for other platforms
    (This guide was written for Linux, but is probably usable on Windows/MacOS
    as well)
*   [Qt Creator](qtcreator.md) - Using Qt Creator as an IDE or GUI debugger
*   [Setting up Visual Studio Code](vscode.md) - Visual Studio Code
*   [EMACS Notes](emacs.md) - EMACS commands/styles/tool integrations
*   [Atom](atom.md) - Atom multi-platform code editor

### Git
*   [Git Cookbook](git_cookbook.md) - A collection of git recipes for common
    tasks
*   [Git Tips](git_tips.md) - More git tips

### Clang
*   [Clang Compiler](clang.md) - General information on the clang compiler, used
    by default on Mac and Linux
*   [Clang Tool Refactoring](clang_tool_refactoring.md) - Leveraging clang tools
    to perform refactorings that are AST-aware
*   [The Clang Static Analyzer](clang_static_analyzer.md) - How to enable static
    analysis at build time
*   [Clang Code Coverage Wrapper](clang_code_coverage_wrapper.md) - Enable Clang
    code coverage instrumentation for a subset of source files.
*   [Writing Clang Plugins](writing_clang_plugins.md) - Don't write a clang
    plugin, but if you do, read this
*   [Updating Clang](updating_clang.md) - Updating the version of Clang used to
    build
*   [Using clang-format on Chromium C++ Code](clang_format.md) - Various ways to
    invoke clang-format on C++ code
*   [Clang Tidy](clang_tidy.md) - Support for the `clang-tidy` tool in Chromium
*   [Updating Clang Format Binaries](updating_clang_format_binaries.md) - How up
    update the clang-format binaries that come with a checkout of Chromium

### General Development
*   [Contributing to Chromium](contributing.md) - Reference workflow process for
    contributing to the Chromium code base.
*   [Commit Checklist](commit_checklist.md) - Streamlined checklist to go
    through before uploading CLs on Gerrit.
*   [Code Reviews](code_reviews.md) - Code review requirements and guidelines
*   [Respectful Code Reviews](cr_respect.md) - A guide for code reviewers
*   [Respectful Changes](cl_respect.md) - A guide for code authors
*   [LUCI Migration FAQ](luci_migration_faq.md) - FAQ on Buildbot-to-LUCI
    builder migration for Chromium
*   [Tour of Continuous Integration UI](tour_of_luci_ui.md) - A tour of our
    the user interface for LUCI, our continuous integration system
*   [Parsing Test Results](parsing_test_results.md) - An introduction for how to
    understand the results emitted by polygerrit and CI builds.
*   [Closure Compilation](closure_compilation.md) - The _Closure_ JavaScript
    compiler
*   [Threading and Tasks in Chrome](threading_and_tasks.md) - How to run tasks
    and handle thread safety in Chrome.
*   [Callback<> and Bind()](callback.md) - All about Callbacks, Closures, and
    Bind().
*   [Views Platform Styling](ui/views/platform_style.md) - How views are styled
    to fit in different native platforms
*   [Tab Helpers](tab_helpers.md) - Using WebContents/WebContentsObserver to add
    features to browser tabs.
*   [Adding third_party Libraries](adding_to_third_party.md) - How to get code
    into third_party/
*   [Graphical Debugging Aid for Chromium Views](graphical_debugging_aid_chromium_views.md) -
    Visualizing view trees during debugging
*   [Bitmap Pipeline](bitmap_pipeline.md) - How bitmaps are moved from the
    renderer to the screen.
*   [base::Optional](optional.md) - How to use `base::Optional` in C++ code.
*   [Using the Origin Trials Framework](origin_trials_integration.md) - A
    framework for conditionally enabling experimental APIs for testing.
*   [`ClientTagBasedModelTypeProcessor` in Unified Sync and Storage](sync/uss/client_tag_based_model_type_processor.md) -
    Notes on the central data structure used in Chrome Sync.
*   [Chrome Sync's Model API](sync/model_api.md) - Data models used for syncing
    information across devices using Chrome Sync.
*   [Ozone Overview](ozone_overview.md) - Ozone is an abstraction layer between
    the window system and low level input and graphics.
*   [Optimizing Chrome Web UIs](optimizing_web_uis.md) - Notes on making webuis
    more performant
*   [Adding a new feature flag in chrome://flags](how_to_add_your_feature_flag.md) - Quick
    guide to add a new feature flag to experiment your feature.
*   [Guidelines for considering branch dates in project planning](release_branch_guidance.md) -
    What to do (and not to do) around branch dates when scheduling your project
    work.
*   [WebUI Explainer](webui_explainer.md) - An explanation of C++ and JavaScript
    infrastructural code for Chrome UIs implemented with web technologies (i.e.
    chrome:// URLs).
*   [Watchlists](infra/watchlists.md) - Use watchlists to get notified of CLs
    you are interested in.

### Testing
*   [Running and Debugging Web Tests](testing/web_tests.md)
*   [Writing Web Tests](testing/writing_web_tests.md) - Web Tests using
    `content_shell`
*   [Web Test Expectations and Baselines](testing/web_test_expectations.md) -
    Setting expected results of web tests.
*   [Web Tests Tips](testing/web_tests_tips.md) - Best practices for web tests
*   [Web Tests with Manual Fallback](testing/web_tests_with_manual_fallback.md) -
    Writing tests that simulate manual interventions
*   [Extending the Web Test Framework](how_to_extend_web_test_framework.md)
*   [Fixing Web Test Flakiness](testing/identifying_tests_that_depend_on_order.md) -
    Diagnosing and fixing web test flakiness due to ordering dependencies.
*   [Running Web Tests using `content_shell`](testing/web_tests_in_content_shell.md) -
    Running web tests by hand.
*   [Web Platform Tests](testing/web_platform_tests.md) - Shared tests across
    browser vendors
*   [Using Crashpad with `content_shell`](testing/using_crashpad_with_content_shell.md) -
    Capture stack traces on layout test crashes without an attached debugger
*   [Test Descriptions](testing/test_descriptions.md) - Unit test targets that can be
    built, with associated desciptions.
*   [Fuzz Testing](../testing/libfuzzer/README.md) - Fuzz testing in Chromium.
*   [IPC Fuzzer](testing/ipc_fuzzer.md) - Fuzz testing of Chromium IPC interfaces.
*   [Running Chrome tests with AddressSanitizer (asan) and LeakSanitizer (lsan)](testing/linux_running_asan_tests.md) -
    Run Chrome tests with ASAN and LSAN builds to detect addressability issues and memory leaks.
*   [Code Coverage](testing/code_coverage.md) - Code coverage for Chromium.
*   [Code Coverage in Gerrit](testing/code_coverage_in_gerrit.md) - Per-CL code
    coverage in Gerrit to assist code reviews.

### GPU-related docs
*   [GPU Pixel Wrangling](gpu/pixel_wrangling.md) - Instructions for GPU
    pixel wrangling (the GPU sheriffing rotation).
*   [Debugging GPU related code](gpu/debugging_gpu_related_code.md) - Hints for
    debugging GPU- and graphics-related code.
*   [GPU Testing](gpu/gpu_testing.md) - Description of Chromium's GPU testing
    infrastructure.
*   [GPU Bot Details](gpu/gpu_testing_bot_details.md) - In-depth description of
    how the bots are maintained.

### Misc Linux-Specific Docs
*   [Linux Proxy Config](linux_proxy_config.md) - Network proxy sources on Linux
*   [Debugging SSL on Linux](linux_debugging_ssl.md) - Tips on debugging SSL
    code in Linux
*   [Linux Cert Managment](linux_cert_management.md) - Managing X.509
    Certificates in Linux
*   [Tips for Debugging on Linux](linux_debugging.md)
*   [Linux GTK Theme Integration](linux_gtk_theme_integration.md) - Having
    Chrome match the GTK+ theme.
*   [Browser Plugins on Linux](linux_plugins.md) - A collection of links to
    information on how browser plugins work on Linux
*   [Linux Crash Dumping](linux_crash_dumping.md) - How Breakpad uploads crash
    reports to Google crash servers.
*   [Linux Minidump to Core](linux_minidump_to_core.md) - How to convert a
    Breakpad-generated minidump files to a core file readable by most debuggersx
*   [Linux Sandbox IPC](linux_sandbox_ipc.md) - The lower level UPC system used
    to route requests from the bottom of the call stack up into the browser.
*   [Linux Dev Build as Default Browser](linux_dev_build_as_default_browser.md) -
    How to configure a Dev build of Chrome as the default browser in Linux.
*   [Linux Chromium Packages](linux_chromium_packages.md) - Packages of Chromium
    browser (not Chrome) provided by some Linux distributions.
*   [`seccomp` Sandbox Crash Dumping](seccomp_sandbox_crash_dumping.md) - Notes
    on crash dumping a process running in a seccomp sandbox.
*   [Linux Password Storage](linux_password_storage.md) - Keychain integrations
    between Chromium and Linux.
*   [Linux Sublime Development](sublime_ide.md) - Using Sublime as an IDE
    for Chromium development on Linux.
*   [Building and Debugging GTK](linux_building_debug_gtk.md) - Building
    Chromium against GTK using lower optimization levels and/or more debugging
    symbols.
*   [Debugging GTK](linux_debugging_gtk.md) - Using the GTK Debug packages and
    related tools.
*   [Chroot Notes](using_a_linux_chroot.md) - Setting up a chroot to work around
    libfreetype differences in some versions of Linux.
*   [Linux Sandboxing](linux_sandboxing.md) - The Linux multi-process model to
    isolate browser components with different privileges.
*   [Zygote Process](linux_zygote.md) - How the Linux Zygote process, used to
    spawn new processes, works.
*   [Running Web Tests on Linux](testing/web_tests_linux.md) - Linux-specific
    instructions for running web tests.
*   [Linux Sysroot Images](linux_sysroot.md) - How builds use libraries on Linux
*   [Linux Hardware Video Decoding](linux_hw_video_decode.md) - Enabling
    hardware video decode codepaths on Linux

### Misc MacOS-Specific Docs
*   [Using CCache on Mac](ccache_mac.md) - Speed up builds on Mac using ccache
    with clang/ninja
*   [Cocoa tips and tricks](cocoa_tips_and_tricks.md) - A collection of idioms
    used when writing Cocoa views and controllers
*   [MacViews Release Plan](ui/views/macviews_release.md)

### Misc Windows-Specific Docs
*   [Handling cygwin rebaseall failures](cygwin_dll_remapping_failure.md)
*   [Hacking on ANGLE in Chromium](angle_in_chromium.md) - OpenGL ES 2.0 built
    on top of DirectX
*   [Windows Split DLLs](windows_split_dll.md) - Splitting `chrome.dll` into
    multiple dlls to work around toolchain limitations on Windows.

### Misc Android-Specific Docs
*   [Google Play Services in Chrome for Android](google_play_services.md)
*   [Accessing C++ Enums in Java](android_accessing_cpp_enums_in_java.md) - How
    to use C++-defined enums in Java code
*   [Profiling Content Shell on Android](profiling_content_shell_on_android.md) -
    Setting up profiling for `content_shell` on Android
*   [Working Remotely with Android](working_remotely_with_android.md) - Building
    on a remote machine for an Android device connected to your local machine
*   [Android Test Instructions](testing/android_test_instructions.md) - Running a build
    on an Android device or emulator.
*   [Android Debugging](android_debugging_instructions.md) - Tools and tips for
    how to debug Java and/or C/C++ code running on Android.
*   [Android Logging](android_logging.md) - How Chrome's logging API works with
    `android.util.Log` on Android, and usage guidelines.
*   [Chromoting Android Hacking](chromoting_android_hacking.md) - Viewing the
    logs and debugging the Chrome Remote Desktop Android client.
*   [Android Java Static Analysis](../build/android/docs/lint.md) - Catching
    Java related issues at compile time with the 'lint' tool.
*   [Java Code Coverage](../build/android/docs/coverage.md) - Collecting code
    coverage data with the EMMA tool.
*   [Android BuildConfig files](../build/android/docs/build_config.md) -
    What are .build_config files and how they are used.
*   [Android App Bundles](../build/android/docs/android_app_bundles.md) -
    How to build Android app bundles for Chrome.
*   [Dynamic Feature Modules (DFMs)](android_dynamic_feature_modules.md) - How
    to create dynamic feature modules.
*   [Chrome for Android UI](ui/android/overview.md) - Resources and best practices for
    developing UI

### Misc iOS-Specific Docs
*   [Continuous Build and Test Infrastructure for Chromium for iOS](ios/infra.md)
*   [Opening links in Chrome for iOS](ios/opening_links.md) - How to have your
    iOS app open links in Chrome.
*   [User Agent in Chrome for iOS](ios/user_agent.md) - Notes on User Agent
    strings using Chrome for iOS.
*   [Running iOS test suites locally](ios/testing.md)

### Misc Chrome-OS-Specific Docs
*   [Setting up captive portals and other restrictive networks](login/restrictive_networks.md)
*   [Enterprise Enrollment](enterprise/enrollment.md)
    *   [Kiosk mode and public sessions](enterprise/kiosk_public_session.md)
*   [Debugging UI in OOBE/login/lock](login/ui_debugging.md)
*   [Chrome Logging on Chrome OS](chrome_os_logging.md)

### Misc WebUI-Specific Docs
*   [Creating WebUI Interfaces in components/](webui_in_components.md) How to
    create a new WebUI component in the `components/` directory.

### Media
*   [Audio Focus Handling](media/audio_focus.md) - How multiple MediaSession
    audio streams interact
*   [Autoplay of HTMLMediaElements](media/autoplay.md) - How HTMLMediaElements
    are autoplayed.
*   [Piranha Plant](piranha_plant.md) - Future architecture of MediaStreams
*   [Video Encode Accelerator Tests](media/gpu/veatest_usage.md) - How to
    use the accelerated video encoder test program.
*   [Video Decoder Tests](media/gpu/video_decoder_test_usage.md) - Running the
    video decoder tests.
*   [Video Decoder Performance Tests](media/gpu/video_decoder_perf_test_usage.md) -
    Running the video decoder performance tests.

### Accessibility
*   [Accessibility Overview](accessibility/overview.md) - Overview of
    accessibility concerns and approaches in Chromium.
*   [Accessibility Tests](accessibility/tests.md) - Where to find
    accessibility-related tests in the codebase.
*   [ChromeVox on Chrome OS](accessibility/chromevox.md) - Enabling spoken
    feedback (ChromeVox) on Chrome OS.
*   [ChromeVox on Desktop Linux](accessibility/chromevox_on_desktop_linux.md) -
    Enabling spoken feedback (ChromeVox) on desktop Linux.
*   [Offscreen, Invisible and Size](accessibility/offscreen.md) - How Chrome
    defines offscreen, invisible and size in the accessibility tree.
*   [Text to Speech](accessibility/tts.md) - Overview of text to speech in
    Chrome and Chrome OS.
*   [BRLTTY in Chrome OS](accessibility/brltty.md) - Chrome OS integration with
    BRLTTY to support refreshable braille displays
*   [PATTS on Chrome OS](accessibility/patts.md) - Notes on the PATTS speech
    sythesis engine used on Chrome OS
*   [VoiceOver](ios/voiceover.md) - Using Apple's VoiceOver feature with
    Chromium on iOS.

### Memory
*   [Memory Overview](memory/README.md)

### Memory Infrastructure Timeline Profiling (MemoryInfra)
*   [Overview](memory-infra/README.md)
*   [GPU Profiling](memory-infra/probe-gpu.md)
*   [Adding Tracing to a Component](memory-infra/adding_memory_infra_tracing.md)
*   [Enabling Startup Tracing](memory-infra/memory_infra_startup_tracing.md)
*   [Memory Usage in CC](memory-infra/probe-cc.md)
*   [Memory Benchmarks](memory-infra/memory_benchmarks.md)
*   [Heap Profiling](memory-infra/heap_profiler.md)

### Misc
*   [Useful URLs](useful_urls.md) - A collection of links to various tools and
    dashboards
*   [ERC IRC](erc_irc.md) - Using ERC in EMACS to access IRC
*   [Kiosk Mode](kiosk_mode.md) - Simulating kiosk mode.
*   [User Handle Mapping](user_handle_mapping.md) - Names of developers across
    Chromium/IRC/Google
*   [Documentation Best Practices](documentation_best_practices.md)
*   [Documentation Guidelines](documentation_guidelines.md)
*   [Chromium Browser vs Google Chrome](chromium_browser_vs_google_chrome.md) -
    What's the difference between _Chromium Browser_ and _Google Chrome_?
*   [Google Chrome branded builds](google_chrome_branded_builds.md)
*   [Proxy Auto Config using WPAD](proxy_auto_config.md) - How WPAD servers are
    used to automatically set proxy settings.
*   [Installing Chromium OS on VMWare](installation_at_vmware.md) - How to
    install Chromium OS on VMWare.
*   [User Data Directory](user_data_dir.md) - How the user data and cache
    directories are determined on all platforms.

### Mojo &amp; Services
*   [Intro to Mojo &amp; Services](mojo_and_services.md) - Quick introduction
    to Mojo and services in Chromium, with examples
*   [Mojo API Reference](/mojo/README.md) - Detailed reference documentation for
    all things Mojo
*   [The Service Manager &amp; Services](/services/service_manager/README.md) -
    Services system overview, API references, example services
*   [Service Development Guidelines](/services/README.md) - Guidelines for
    service development in the Chromium tree
*   [Servicifying Chromium Features](servicification.md) - General advice for
    integrating new and existing subsystems into Chromium as services
*   [Converting Legacy IPC to Mojo](mojo_ipc_conversion.md) - Tips and common
    patterns for practical IPC conversion work
*   [Mojo “Style” Guide](security/mojo.md) - Recommendations for best practices
    from Mojo and IPC reviewers

### WebXR
*   [Running OpenVR Without Headset](xr/run_openvr_without_headset.md) -
    Instructions for running OpenVR on Windows without a headset

### Probably Obsolete
*   [TPM Quick Reference](tpm_quick_ref.md) - Trusted Platform Module notes.
*   [System Hardening Features](system_hardening_features.md) - A list of
    current and planned Chrome OS security features.
*   [WebView Policies](webview_policies.md)
*   [Linux Profiling](linux_profiling.md) - How to profile Chromium on Linux
*   [Linux Graphics Pipeline](linux_graphics_pipeline.md)
*   [Linux `SUID` Sandbox](linux_suid_sandbox.md) - Sandboxing renderers using a
    SUID binary on Linux
*   [Linux `SUID` Sandbox Development](linux_suid_sandbox_development.md) -
    Development on the above system.
*   [Linux PID Namespace Support](linux_pid_namespace_support.md)
*   [Vanilla msysgit workflow](vanilla_msysgit_workflow.md) - A workflow for
    using mostly vanilla git on Windows.
*   [Old Chromoting Build Instructions](old_chromoting_build_instructions.md)
*   [Old Options](chrome_settings.md) - Pre-Material Design chrome://settings
    notes.
