# The browser process should not handle messages from web content

![alt text](good-bad-ipc.png "Safe flow of IPC messages from renderer to
browser, via reviewed APIs; together with two example unsafe flows via
postMessage and via unreviewed APIs")

(drawing source
[here](https://docs.google.com/drawings/d/1SmqvOvLY_DnDxeJHKQRB3rACO0aVSHpyfTycV2v1P1w/edit?usp=sharing))

Sometimes features are proposed in which the Chrome user interface (in the
browser process) handles messages directly from web content (JavaScript, HTML
etc.). For example, this could be done using the `postMessage` APIs which have
been put in place for Android WebView apps. This is not allowed, because:

* Overall system security relies on simple and predictable security properties.
  Adding extra message channels causes complexity, non-discoverability and
  non-predictability.
* Chrome's security strategy relies on isolating web content using sandboxed
  renderer processes and site isolation. Any communication outside of that
  renderer process presents a risk of a sandbox escape. All such communication
  has to be via Mojo such that the `mojom` interface definition files go through
  our [IPC security review process](mojo.md) (and will benefit from other future
  Mojo security improvements).
* Websites are untrustworthy. TLS can’t guarantee the provenance of a website —
  even pinning has limits — and so you must assume any messages from websites
  are malicious. Processing such messages in the browser process in C++ is
  likely a violation of the [Rule of Two](rule-of-2.md) and is extremely
  dangerous.
* Even if you can comply with the Rule of Two (for example by using a safe
  language) it's simply difficult to produce robust APIs that are safe against
  malicious data: the open web platform [API review
  process](https://www.chromium.org/blink/launching-features) is designed to
  flush out any concerns. Any APIs or functionality accessible to web content
  therefore needs to go via that process to give the best chance of spotting
  danger.
* There are non-security concerns: It does not comply with the spirit of an open
  web platform which should be equally available on all user agents.

In order to support WebView, WebLayer, and CCT, APIs exist in Chrome to
establish web message channels between the embedding application and web page.
These exist only to support these "embedding the web" scenarios, which are often
used to build site- or purpose-specific browsers. General browser features
should not use them because of the reasons stated above.

Other mechanisms of bypassing normal processes might include exposing unreviewed
APIs to a component extension, and making its APIs available to web content.
These are similarly not allowed.
