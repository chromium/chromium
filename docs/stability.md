# An Overview of All Things Stability/Crash in Chrome

_current as of May 2026_

Chromium and Chrome care about stability. It's one of
[four core design principles](https://blog.chromium.org/2018/09/how-we-designed-chrome-10-years-ago.html)
(along with speed, simplicity, and security) for building a great browser. The
web browser should work reliably.

Chrome measures stability in many ways, knowing that no single measurement is
both reliable and comprehensive.

This document provides an overview of the different ways Chrome monitors
stability and attempts to improve it.

[TOC]

## Found a crash - report it! {#found-crash}

One way stability problems are identified and fixed is through bug reports. Bug
reports, especially those with steps on how to reproduce the crash, are
invaluable. If you have steps that can reliably cause a crash, or even
occasionally cause a crash, please [report it](https://new.crbug.com/).

## What's a crash? Types of crashes {#types-of-crashes}

Chromium and Chrome try to ensure
[all processes involved in web browsing](https://www.chromium.org/developers/design-documents/multi-process-architecture/)
are stable. Stability comes in many forms. The following are types of crashes
and what the user experiences.

- **Browser process crash**. The browser app disappears. The operating system
may display a message about the app crashing. Often, tabs are automatically
restored when the app is restarted, though any data previously entered into web
pages is lost.

- **Renderer process crash**. A renderer process typically displays a web
page [[1](#renderer-with-webui)]. When it crashes, the user sees "Aw, Snap!
Something went wrong when displaying this web page" (a.k.a., a "sad tab").
Reloading the web page typically will make the page reappear, though any
previously entered data is lost.

- **Extension renderer process crash**. Each Chrome extension runs in an
extension renderer process. When most extensions crash, a user will see a popup
indicating that a crash occurred and asking the user to click to reload the
extension. Extensions that are policy-installed or
[component extensions](https://chromium.googlesource.com/chromium/src/+/main/extensions/docs/component_extensions.md)
will
[automatically restart](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/background/background_contents_service.cc;l=380-420;drc=7fa0c25da15ae39bbd2fd720832ec4df4fee705a).
In these cases, there is no user-visible indication that the extension has
crashed. Also, if the user had windows open for the extension (e.g., to
configure it), those windows will show "sad tabs" and any unsaved data entered
into those windows would be lost.

- **GPU process crash**. The GPU process works with the GPU chip on the device
(if any) to help Chromium and Chrome display and scroll web pages quickly. If it
crashes, all windows go black momentarily while a new process is brought up.
(The GPU process is restarted automatically.) Generally no data is lost during
this operation; however, sometimes state and data can be lost. For example,
Canvas 2D animations will be reset (start from the beginning).

- **Utility process crash**. A utility process is a general name for a helper
process other than those above. The most commonly used utility process is the
network service process. It's designed to silently restart in the case of a
crash without users being aware of it.

- **Browser process stalled or unresponsive [[2](#stalled-terminology)]**,
a.k.a. "Application Not Responding" (ANR). Colloquially, the app is said to have
"frozen." A user with a stalled or unresponsive browser process will not be able
to, for example, click links to switch tabs. Whether to refer to this situation
as a crash is debatable. A stalled or unresponsive browser process is like a
browser process crash in that it makes the browser unusable, where the remedy is
to restart the browser. Sometimes the operating system will offer to do this for
the user, or do it directly without asking the user. Alternatively, in some
cases, if the user waits then the browser process may become responsive.

- **Renderer process stalled or unresponsive [[2](#stalled-terminology)]**.
Again, whether to call a stalled or unresponsive renderer process a crash is
debatable. Typically, the fixes are to reload the web pages
[[1](#renderer-with-webui)] or to wait and hope the renderer process becomes
responsive. A user with a stalled or unresponsive renderer process will not be
able to, for example, enter information in the web page or click most links on
the web page.

- **GPU process stalled or unresponsive [[2](#stalled-terminology)]**. TODO:
describe what the user sees in this case.

- **Operating-system force-closing Chrome**: An operating system may, at times,
close Chrome or offer to close Chrome. This is discussed above in the context of
a stalled or unresponsive browser process. The operating system may close Chrome
for other reasons such as using too much memory or doing too much work while in
the background (not being actively used by the user). As with a stalled or
unresponsive browser process, whether to call these a "crash" and whether they
should be said to be Chrome's fault is debatable.

## How Chrome counts crashes {#counting-crashes}

In addition to user-reported crashes in bug reports, Chrome has several
mechanisms for detecting and counting crashes.

**Crashpad**.
[Crashpad](https://chromium.googlesource.com/crashpad/crashpad/+/main/README.md)
is a crash-reporting system that integrates with Chrome on all operating systems
on which Chrome runs. It watches for crashes and crash-like events (more details
below). It watches all processes used by Chrome: the main browser app process as
well as others. For users who have the metrics/crash reporting toggle enabled,
when Crashpad sees a crash in any process, it attempts to collect some
information about the crash and (subject to throttling) upload the information
to Google servers. This information is put into the "crash database".

**UMA**. UMA is the metrics-reporting system for Chrome. For users who have the
metrics/crash reporting toggle enabled, UMA regularly uploads its information to
Google servers. With respect to crashes, UMA records two key types of events:

- The UMA system monitors all processes used by Chrome, including the main
browser app process. Each process is tracked to see if it stopped running in an
expected or unexpected way. When a process stops unexpectedly, it's colloquially
referred to as a crash. Strictly speaking, the situation may not be a crash. For
example, it could be a power outage for a desktop computer or the battery
running out of power for a mobile device. A UMA "crash" is best referred to as
an **unclean shutdown**. One critical feature of unclean shutdowns (as detected
in UMA) is that they may be crashes that were not caught by Crashpad. For
example, Crashpad may not have been able to spot the crash and upload relevant
information.

- The UMA system also keeps metrics about how Crashpad works. For example, it
records key information about what kinds of crashes Crashpad has detected and
how many crashes were successfully uploaded to Google servers.

**Operating system statistics**. Operating systems can collect information about
apps running on the operating system, how often they crash, and for what reason.
An operating system can also measure ANRs. For example, the Google Play store
can collect such information for users of Chrome on Android (when the relevant
Android-level metrics reporting setting is enabled) and display information to
Chrome developers in the Google Play Console. Likewise, Apple's App Store can
collect such information for users of Chrome on iOS and display information in
Chrome's App Store Connect area. In general, these operating-system monitoring
services are neither better nor worse than the systems above. The
operating-system services may record crashes (and crash-like events) not
recorded elsewhere and may miss crashes that are detected by other systems.

**User feedback**. Users can also
[send feedback about Chrome to Google](https://support.google.com/chrome/answer/95315).
Google reviews this feedback to identify which feedback relates to stability and
uses that feedback to monitor stability.

Other sources of data about crashes are also available. The ones mentioned above
are the most commonly used sources.

Most of these are described in more detail below.

## Reporting crashes via "Crashpad" (to crash database) {#crashpad}

Chrome uses Crashpad to record crashes and upload them to the crash database, if
the user has the metrics/crash reporting toggle enabled. As described earlier,
when the Chrome app starts, Crashpad installs an "exception/signal handler".
This handler watches for the app to crash. If possible, it takes a snapshot of
what the app was doing when it crashed. Later, the snapshot will be uploaded to
Google's servers. This snapshot is commonly referred to as a "crash dump". The
data on Google's servers is referred to as the "crash database".

Note: On iOS, there is
[an additional method by which crashes can be uploaded to the crash database](#operating-system-level-counting-ios).
This section about Crashpad describes only the types of crashes that Crashpad
captures and uploads to the crash database.

Crashpad can capture many types of crashes such as invalid memory access and
invalid instructions.

Crashpad can also capture crashes caused by the device running out of virtual
memory. It will not capture crashes caused by the device running out of physical
memory.

For both browser process and helper process crashes, some types of crashes may
not be caught by Crashpad. For example, Crashpad cannot capture the
operating-system force-closing Chrome.

Crashpad can also upload snapshots ("crash dumps") to Google without a crash
occurring. Specifically, a developer can add code to indicate that Chrome should
take a crash dump and upload it to Google. It's literally called "dump without
crashing."

In addition, Chrome monitors and records occurrences when a renderer and GPU
process becomes stalled or unresponsive. Chrome can sometimes record situations
when the browser process stalls or becomes unresponsive, though it does not do
it reliably. Specifically, there is
[a part of Chrome that watches to see if another part of Chrome becomes stalled or unresponsive](https://chromium.googlesource.com/chromium/src/base/+/HEAD/threading/README.md).
Chrome cannot detect all occurrences of this--if Chrome is too stalled then the
watcher cannot run--but for occurrences it detects, Chrome tells Crashpad to
package the information into a crash dump using "dump without crashing" for
upload to the crash database.

Even the real crash reports (as opposed to ones stemming from
stalls/unresponsiveness or from dumps-without-crashing) are not representative
of all crashes. For example, a crash that occurs before the signal handler is
installed will not be seen and uploaded. As another example, there are ways that
Chrome can crash that will leave the app in such a bad state that the signal
handler cannot run successfully to create a crash report.

Crash reports captured and uploaded by Crashpad are detailed! For example, they
typically include the exact line on which the crash occurred and why that line
crashed (e.g., invalid memory access, invalid command). They are also annotated
to distinguish the type of crash report, distinguishing between, say, a regular
crash, a stall/unresponsiveness report, a dump-without-crashing, and
out-of-memory crash, and so on.

While the controls for uploading crash reports and upload UMA data are similar,
there are a number of ways in which the systems differ:

- Alike in that both crash reporting and UMA data are controlled by the same
toggle

- Alike in that both sample their data at the same rate (based on the operating
system and channel)

- Differ in that crash reporting samples crash report events at the rate in
question whereas UMA samples all data from a user (so all data from a user is
reported or not; it's not as a per-event basis)

- Differ in that crash reporting throttles uploads so an individual user cannot
report too many crashes too often (based on operating system and channel); UMA
does not throttle

- Differ in that crash uploads get
[throttled on the server](http://go/crash-report-throttling-server), if there
are too many for a particular operating system, or <operating system, version>
tuple (this happens rarely though)

Querying the crash database to focus on particular types of crashes can be
difficult. Here is [an example query](http://shortn/_yEgrsq543q) (Google
employees only) for how to query the crash database to look at browser process
crashes, excluding other process crashes and excluding stall/unresponsiveness
reports.

### Desktop Nuances {#crashpad-desktop}

Crash report uploads on Windows, macOS, Linux, and ChromeOS are throttled. In
other words, the same user cannot upload many crash reports in a short time.
There's a quota. As such, the crash database may underestimate the frequency of
certain types of crashes (ones that tend to appear in a row / in a cluster).
This quota is not applied on Android or iOS.

### ChromeOS Nuances {#crashpad-chromeos}

The ChromeOS operating system kernel can watch the Chrome process and recognize
when it crashes. If it sees Chrome crash in a way that was not handled by
Crashpad, it produces a crash report indicating this. As such, it can be said
that the crash database has a more complete reflection of Chrome's stability on
ChromeOS than on other platforms. That said, the crash reports produced in these
cases (when Crashpad didn't handle the crash) have very few details.

### iOS Nuances {#crashpad-ios}

Due to iOS operating system restrictions, Crashpad on iOS cannot capture certain
types of crashes that it can capture on other operating systems. For example, on
Android, when the signal handler detects a crash, it launches another process to
examine the crashing Chrome app. This makes it more likely to successfully
create a crash report on Android and upload it. On iOS, due to iOS operating
system restrictions, the crash handling must be done within the Chrome app
process. This makes it more difficult to reliably capture and upload a crash
report.

Crash information on iOS is written to an intermediate file, then converted to
the final file format ("minidump") and uploaded when possible. This means the
uploads are delayed compared with other operating systems where the crash report
is immediately uploaded (if possible) in a separate process.

See
[the Crashpad documentation for iOS for more details](https://chromium.googlesource.com/crashpad/crashpad/+/HEAD/doc/ios_overview_design.md)
on all of the above and more.

## Counting unclean shutdowns ("crashes") in UMA {#uma-crashes}

Note: UMA metrics often refer to stability problems as "crashes" but they are
more accurately called "unclean shutdowns".

Unclean shutdowns in UMA are a measure of how often Chrome or one of its helper
processes did not shut down / turn off cleanly. The UMA information about
unclean shutdowns (by process) are all stored in a single UMA histogram:
`Stability.Counts2`. UMA information is eligible to be uploaded only if the
metrics/crash reporting toggle is enabled.

An unclean shutdown of the browser app is detected using a beacon. As Chrome is
shutting down (or, on mobile, in some conditions, being backgrounded), a beacon
is written indicating that Chrome has shut down cleanly. When Chrome is started,
the beacon is read. If the beacon does not indicate that the last shutdown was
clean, then the last shutdown must have been unclean, such as a crash. Sometime
during startup (or, on mobile, in some conditions, foregrounding), the beacon is
reset, indicating that Chrome is listening for unclean shutdowns again. This
method can reliably capture most types of crashes, including many that do not
get reported to the crash database. For example, unclean shutdowns for the
browser app process include both crashes as well as times when the operating
system or user "force quit" (a.k.a. "kills") the Chrome browser app process,
which is another way to make Chrome shutdown uncleanly. An operating system may
force quit the Chrome browser app if it's using too much memory. Also, an
operating system may force quit the Chrome browser app process if it's not
responding ("Application Not Responding" a.k.a. ANR). In this way, unclean
shutdowns for the browser process may include times when the browser process
stalled or became unresponsive.

Helper processes are also monitored for unclean shutdowns. These include regular
crashes (due to a bug in Chrome), OOM crashes (when memory limits are exhausted,
due to the behavior of the page, Chrome, or other processes running on the
system), and external causes (such as the user terminating the process via Task
Manager). In short, generally an unclean shutdown is counted whenever a process
wasn't shut down intentionally by the browser process [[3](#failed-launch)], and
also sometimes is counted when the helper process was shut down intentionally as
well. For example, if a renderer process is closed by the browser process to
free up memory, that's considered an unclean shutdown of the renderer process in
UMA. Incidentally, UMA has additional information in particular about
[helper process *crashes*](#uma-helper-process-crashes); see below.

While measurements of unclean shutdowns will include many problems with Chrome,
there are a variety of other causes for unclean shutdowns that do not involve
Chrome functioning poorly. For example, if the operating system as a whole
crashes, that will cause Chrome to not shut down cleanly. Likewise, if the
device loses power (power outage or battery fully discharged), Chrome will also
recognize an unclean Chrome browser process shutdown. Unclean shutdowns caused
by these forces are outside of Chrome's control.

Thus, unclean shutdowns are best used to detect directional changes in stability
between Chrome versions or over time. The absolute value is not particularly
meaningful. For example, the absolute count of unclean browser app shutdowns
depends heavily on how reliably the unclean shutdown beacon is and how much of
the browser process lifetime is monitored for unclean shutdowns. If/when these
change, the absolute count can change substantially. Moreover, even relative
values need to be interpreted with care: a 20% increase in unclean browser app
shutdowns does not indicate a 20% increase in Chrome stability problems, as a
sizable fraction of unclean shutdowns are not due to Chrome.

When UMA uploads information indicating that an unclean shutdown occurred, it
does not provide additional information. It's best to think about this way of
counting crashes as merely providing a number: the number of unclean shutdowns.
The UMA information does not provide more detailed indications about which code
crashes and why.

### Android Nuances {#uma-crashes-android}

In Chrome on Android, Chrome considers an unclean shutdown of the browser
process to have occurred only if it happens when Chrome is in the foreground.
It's expected that Android will force Chrome to close while Chrome is in the
background. These forced closures should not be and are not considered
unexpected shutdowns. This behavior is implemented by changing how the unclean
shutdown beacon works: it watches for unclean shutdowns between the time Chrome
comes to the foreground and Chrome goes to the background, not between startup
and shutdown.

### iOS Nuances {#uma-crashes-ios}

Chrome on iOS monitors for unclean shutdowns under more conditions than Chrome
on Android does, but does not monitor continuously. Chrome monitors for unclean
shutdown of the browser process when Chrome is in the foreground. Chrome also
monitors for unclean shutdowns when Chrome is launched in the background to do
background work. It ceases to monitor for unclean shutdowns when the background
work completes. Generally, it's expected that iOS will force Chrome to close
while Chrome is in the background. If these forced closures happen during a time
when Chrome does not mind being closed, these forced closures are not considered
unexpected shutdowns.

Separately, due to iOS restrictions,
[Chrome's architecture on iOS differs greatly from its architecture on other operating systems](https://chromium.googlesource.com/website/+/HEAD/site/developers/design-documents/layered-components-design/index.md).
Thus, many situations that occur on other operating systems do not occur on iOS.
The only stability metrics measured in UMA are browser process unclean shutdowns
and renderer process unclean shutdowns. (There is conceptually a renderer
process to display web content on iOS; the process is run by the operating
system, not by Chrome. Chrome records a renderer process unclean shutdown when
it is notified that the iOS-launched renderer process is no longer running.)

### UMA Context {#uma-extra-information}

UMA data, including UMA data about unclean shutdowns, comes with additional
information about Chrome such as the type of device it's running on, the current
Chrome version, and so on. Generally this information should be correct.
However, there are some known situations where it may not be:

- If the Chrome browser crashes, Chrome will attempt to indicate that in a UMA
record that contains the Chrome version at the time of the crash. There is code
to this effect. However, this code has been buggy in the past and some edge
cases might remain.

- On Android, Chrome attempts to indicate the manner in which it is running,
e.g., as a standalone browser or as a Chrome Custom Tab or something else. This
information is not reliably associated with the crash information in UMA; the
attached information may not be correct.

## Counting becomes stalls / unresponsiveness in UMA {#uma-stalls}

Chrome has
[a system for detecting stalls or unresponsiveness](https://chromium.googlesource.com/chromium/src/+/HEAD/base/threading/README.md)
in every process, including the browser process. This system functions on
Windows, Mac, Linux, and ChromeOS. When such a situation is detected, it's
reported to UMA in a histogram in the family `HangWatcher.IsThreadHung.*`.

## Counting Crashpad-measured crashes in UMA {#uma-crashpad-histograms}

There are UMA histograms that reflect how Crashpad behaves. They all start with
`Crashpad.`. For example, the UMA histogram
`Crashpad.CrashUpload.AttemptSuccessful` contains the number of crash dumps that
were uploaded successfully.

These UMA histograms are not typically used to count crashes or measure
stability in general. The histograms are typically used to troubleshoot or debug
Crashpad itself.

## Other stability metrics in UMA {#uma-other-stability-metrics}

### Process exit codes {#uma-process-exit-codes}

When a process exits--either via a crash or "cleanly" (without a crash)--it has
an "exit code" indicating its status. These exit codes are reported to UMA. The
following UMA histograms are relevant. For technical reasons, not all are
recorded on all operating systems.

- `Stability.BrowserExitCodes` - for the browser process

- `CrashExitCodes.Renderer` - for renderer processes

    - Also, for renderer processes, the set of UMA histograms that start with
    `Stability.RendererAbnormalTermination2` report a summarized (grouped)
    version of renderer process exit codes.

- `CrashExitCodes.Extension` - for extension renderer processes

- `GPU.GPUProcessExitCode` - for GPU processes

- `ChildProcess.Crashed.UtilityProcessExitCode` - for utility (a.k.a. helper)
processes

These exit codes can indicate, for example, if the process ran out of memory,
was stalled or unresponsive and thus forced to close, or tried to access memory
it wasn't allowed to access. For non-browser process crashes, these crashes
should also have been caught by Crashpad and uploaded to the crash database. For
browser-process crashes, many of these exit codes cannot be caught by Crashpad
and thus would not appear in the crash database.

These are all considered unclean shutdowns, as counted in the UMA histogram
`Stability.Counts2`.

### Out-of-memory {#uma-out-of-memory}

[Process exit codes above](#uma-process-exit-codes) reflect out-of-memory
crashes. (On some operating systems, there's an exit code for Chrome that
indicates out-of-memory.) On some operating systems, there are additional UMA
metrics that indicate an out-of-memory crash occurred:

- `Memory.OOMKills.Count` - for ChromeOS

- `Memory.OOMKills.Daily` - for ChromeOS

- `Stability.Android.ProcessedCrashCounts` - for Android

### Helper process crashes {#uma-helper-process-crashes}

When a non-browser process crashes, it should be able to be caught by Crashpad
and uploaded to the crash database.

Chrome also catches a non-browser-process crash, processes the event, and
records some statistics about it in UMA data. On Android, this information is
placed in the UMA histogram `Stability.Android.ProcessedCrashCounts`.

#### Utility process crashes by types {#uma-utility-process-crashes-by-type}

The histogram `ChildProcess.Crashed.UtilityProcessHash` counts crashes (either
true crashes or unclean shutdowns of the process) by utility process type. To
put these crash counts into context, compare them again the number of times
each utility process type was launched as measured by
`ChildProcess.Launched.UtilityProcessHash`.

## Counting crashes at the Operating-System-Level {#operating-system-level-counting}

Operating systems monitor only app-level stability, i.e., whether the Chrome
browser app crashed or became stalled or unresponsive ("Application Not
Responding"). Operating systems do not record the inner workings of Chrome such
as renderer process or GPU process stability. Whether information is collected
by the operating system's maker is usually configurable by a setting within the
operating system.

Also worth mentioning: operating systems are not aware of Chrome experiments.
The crashes and metrics they collect do not include information about the
experiments a user was in.

The organization providing the operating system decides what information, if
any, to share with Chrome developers about Chrome's behavior. Such information
is typically shared, if at all, in an aggregated form through a developer
portal, such as Android's Play Store or iOS's App Store.

### iOS Nuances {#operating-system-level-counting-ios}

For users whose devices are configured to share analytics with Apple and further
configured to allow Apple to share analytics with app developers, Chrome can
request and receive the information that the operating system observed about
Chrome's stability on this device. The information includes, for example, the
number of foreground exits. Foreground exits can be a crash such as "Invalid
memory access" or can be a time when iOS decided to force Chrome to stop running
(such as "App used too much memory"). The information also includes the number
of occurrences of browser process stalls/unresponsiveness, periods where the
browser was not responding to input.

This information is provided to Chrome using
[iOS's MetricKit framework](https://developer.apple.com/documentation/MetricKit).
For users who have Chrome's metrics/crash reporting toggle enabled, Chrome can
send this information to Google in two forms:

- Chrome will upload information to the crash database about times when Chrome
closed unexpectedly.

    - It includes crashes: iOS can watch for times when Chrome crashes. It will
    notice crashes even in situations when Chrome cannot capture and record the
    crash itself. This should make the data on Chrome browser app crashes more
    comprehensive. iOS will tell Chrome, on a later restart, that Chrome crashed
    on a previous run. MetricKit provides to Chrome some basic information about
    the crash. Chrome will upload a crash report with this MetricKit-provided
    data to the crash database.

    - It also includes other unexpected shutdowns. iOS may decide to kill the
    app if, for example, Chrome is not responding. MetricKit will also provide a
    report to Chrome in these cases that will be uploaded to the crash database.
    These cases in which iOS decides to force the app to quit are typically not
    captured by Crashpad.

- Chrome will upload statistics provided by MetricKit using Chrome's UMA metrics
reporting system. These statistics are stored in UMA histograms with names that
start with `IOS.MetricKit.`.

At the time of writing, some aspects of MetricKit's operation are unknown. For
example, it's unclear how quickly iOS will provide MetricKit data to Chrome.
It's also unclear whether the information MetricKit provides is complete or
selective; i.e., will MetricKit decide not to tell Chrome about some crashes
that it has observed.

## What Chrome does with crash information {#actions}

Chrome developers use all these methods for detecting and measuring crashes to
improve Chrome's stability.

Often, the crashes recorded by Crashpad and thus put into the crash database are
the most useful. They have the most detail: a crash usually points directly to
the line of code that crashed. Thus, they are the most actionable.

Different people handle crash information differently.

### Code owners

Code owners are likely to use the crash database. If a crash appears that looks
like it's coming from their code, they will investigate and improve the code /
fix the bug.

### Experimenters

Experimenters are likely to use both UMA data and the crash database. Both UMA
data and the crash database data can be sliced by the experiment group. If
either shows a regression in stability, the cause will be investigated and
hopefully fixed before rolling out the change to more people. Experimenters
check both these ways of measuring stability because, as described above, some
types of crashes or instability are measured only by one and not the other.

### Stability rotation

Generally, there are people who keep an eye on Chrome's stability. They use all
the sources above. Often they're looking at graphs over time. (Did things get
worse? If so, what?) They also help triage stability problems, making sure, for
example, the crash dump or bug report gets routed to the right code owner to
look into and fix.

### Release managers

Release managers are the people who push a new Chrome version (a "release") to
users. Release managers use crash rates as an input to make decisions about when
a new Chrome version is ready to be distributed widely. They typically look at
the relative change in stability from the old version to the new version on a
given operating system, e.g., an increase or decrease of 10% in the number of
crashes.

## Warnings

### Normalize crash counts {#normalize-crash-counts}

Chrome usage can change over time such as over weekends or holidays. Raw crash
counts are generally not useful. They're only useful to compare one number with
another number, assuming those numbers were collected at the same time on the
same operating system. To make crash counts more meaningful, it's a good idea to
normalize them to take into account how much Chrome was used during the time the
data was collected. For Google employees, many tools will present normalized
numbers in addition to or instead of unnormalized ones. This document
intentionally does not describe different approaches to normalization as that
information is confidential.

### Do not compare across operating systems {#no-cross-operating-system-comparisons}

<section class="zippy" markdown="1">

Warning: Comparing crash rates across operating systems is not a fair
comparison. Expand for details.

Crashpad reporting, UMA reporting, and operating system statistics about crashes
can have different reliability levels on different operating systems.

For example, Crashpad might catch and upload some types of crashes on Android
that it cannot capture or upload on iOS.

Likewise, UMA "crash" rates (actually unclean shutdowns) may reflect more about
the likelihood of the device running out of power than Chrome or some part of
Chrome crashing. Also, UMA unclean shutdown rates may also reflect how the
unclean shutdown detection system is implemented, which can vary by operating
system. For both of these, see [details above](#uma-crashes). Obviously, these
rates will differ substantially across operating systems.

In addition, the percent of users who have the metrics/crash reporting toggle
enabled varies by operating system. As such, comparing raw numbers of crashes is
inappropriate.

Furthermore, different operating systems have different prevalence of
[automation / bots, as discussed below](#bots).

Finally, heterogeneity of devices also makes it difficult to compare fairly
across operating systems. For example, Chrome on iOS needs to run only on iPhone
and iPad devices. These devices have a limited set of specifications and the
quality of their components is controlled by Apple. Meanwhile, Chrome on Android
runs on a variety of devices with an enormous diversity of manufacturers.
Components can range from extremely low-end (slow components, small memory,
unreliable) to high-end.

</section>

### Do not compare across data sources {#no-cross-data-source-comparisons}

Generally different data sources measure different things. Attempts in the past
to compare them have not usually provided much insight.

### Automation {#bots}

Crash reports may include interactions with Chrome due to automation. A "bot"
may drive Chrome by faking taps, typing, and so on. These crash reports may not
reflect any real user experiencing a crash.

It's also likely that crashes that result from automation may be relatively more
common on one operating system than another.

### Malware and antivirus {#malware}

Other programs running on the device, especially if they're running with
elevated privileges, can cause Chrome to crash. Sometimes these crashes will
appear in the various data sources as if they were problems with Chrome.

### Bias due to connectivity {#wifi-vs-wireless}

<section class="zippy" markdown="1">

Stability information may be unrepresentative due to operating-system- or
Chrome-level constraints on when data can be uploaded. Expand for details.

For example, Chrome on Android will by default only upload a crash dump when
the network connection is unmetered.

As another example, the UMA data reporting system uploads data less often when
the user is on a wireless network rather than a wifi network.

</section>

## Footnotes

### 1. Renderer with WebUI {#renderer-with-webui}

A renderer can also be used to display the Chrome interface, such as the
chrome://settings page. This happens via a framework called
[WebUI](https://chromium.googlesource.com/chromium/src/+/main/docs/webui/webui_explainer.md).
Renderer process crashes (and hangs) are recorded the same way regardless of
context (displaying a web page or displaying the Chrome interface). In many
systems, it is difficult or impossible to distinguish between these two cases.

### 2. Stalled terminology {#stalled-terminology}

Formerly, a stalled or unresponsive process was called "a hang" or "a hung
process." Use of these older terms remains widespread in Chromium code.

### 3. Failed launch {#failed-launch}

The one exception to this rule is when a helper process fails to launch. Suppose
the browser process tells a helper process to launch. However, imagine that the
helper process doesn't start up successfully and begin communicating back with
the browser process. Intuitively, one might consider this situation an unclean
shutdown of the helper process. However, this situation is not considered an
unclean shutdown or crash; it's instead logged in `Stability.Counts2` only as a
failure to launch.
