# How to use perfetto for both Ash and Lacros

## Perfetto

[Perfetto](https://ui.perfetto.dev) is a project intended to provide a common
platform for performance instrumentation and logging across Chrome and Android.

This tool can be used for in process tracing and system tracing. In case of
ChromeOS running LaCros, it can capture bosh Ash and Lacros processes at the
same time providing developers with good understanding of what happens when
Lacros interacts with Ash through its Wayland compositor, which is called exo.

## Requirements

The [Perfetto UI](https://chrome.google.com/webstore/detail/perfetto-ui/lfmkphfpdbjijhpomgecfikhfohaoine)
extension must be installed.

## Ways to make trace records

There are two ways to create perfetto traces - either via Ash/Chrome or LaCros.

### Tracing via LaCros

1) Ensure that [EnablePerfettoSystemTracing](https://source.chromium.org/chromium/chromium/src/+/71d16454eeec5555df3411d04ea84530c7335c86:services/tracing/public/cpp/tracing_features.cc;l=34)
is enabled (must be enabled by default).
2) Ensure that the Perfetto UI extension is installed.
3) Go to [ui.perfetto.dev](https://ui.perfetto_dev).
4) Choose `Record new trace`.
5) Choose `Chrome OS (system trace)` as your `Target platform`.
6) Select `probes` you are interested in (`Chrome` in our case).
7) Select desired categories.
8) Press `Start recording`.

If you cannot see Ash processes, add one of the categories you can find in
the `CPU` probe. For example, `syscalls`.


### Tracing via Ash/Chrome

Tracing via Ash/Chrome is a bit different. One must
1) Disable Lacros by going to `/etc/chrome_dev.conf` on their device and
commenting out `--enable-features=LacrosOnly`
2) Restart ui
3) Open Ash/Chrome and install/re-install the Perfetto UI extension
4) Add `--enable-features=LacrosOnly` back and also add `--enable-ash-debug-browser`
to `/etc/chrome_dev.conf`.
5) Restart ui.
6) Ensure Ash/Chrome still has the extension (Ash/Chrome can be found in the
launcher page).
7) Open LaCros and prepare a site/page it will load/run.
8) In Ash/Chrome Go to [ui.perfetto.dev](https://ui.perfetto_dev).
9) Choose `Record new trace`.
10) Choose `Chrome OS (system trace)` as your `Target platform`.
11) Select `probes` you are interested in (`Chrome` in our case).
12) Select desired categories.
13) Press `Start recording`.
14) Switch back to LaCros and load the page

If you cannot see Lacros processes, add one of the categories you can find in
the `CPU` probe. For example, `syscalls`.
