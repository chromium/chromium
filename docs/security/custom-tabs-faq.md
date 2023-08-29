# Chrome Custom Tabs Security FAQ

## Should apps use WebView when building a browser?

[No, WebView is not intended as a framework for building browsers, and lacks
security features available in modern
browsers.](https://web.dev/web-on-android/#security-considerations-for-using-webview-as-an-in-app-browser)

## What is the security model for Chrome Custom Tabs?

Chrome Custom Tabs (CCT), and Custom Tabs (CT) more generally, allow
Android app developers to use the user's default browser to
serve embedded web content in their apps.

CT, unlike Android's WebView API, share the same browser state (such as
cookies) with the browser app. Chromium therefore imposes a strict boundary
between the embedding app and the browsing engine, and the app can normally
only get very limited access to web page data and state.

All considered, there are four parties to consider when evaluating Custom Tabs:
the user, the embedding app, the web publisher, and the browser. The native
app chooses how they want to bring the web in their app, and users choose which
apps to install and use.

Given this distinct trust relationship between the embedding app and the user
(which is in general a higher degree of trust than between users and websites
they happen upon in their browser), we accept some data exchange between Chrome
and the underlying app. This is intentional because we believe this
incentivizes apps to use CT rather than WebView, which was [never designed as a
full browser embedding API and has a number of security shortcomings](https://web.dev/web-on-android/#security-considerations-for-using-webview-as-an-in-app-browser).

## What data does Chrome consider permissible for the embedder to have access to?

1. **CCT session specific signals can be shared back to the embedder without user
   action.** Session specific signals are low-entropy signals about the user's
   interaction with the tab or page that do not reveal information about the
   content or identity of the page. Examples of session specific signals include
   [Custom Tab callbacks](https://developer.android.com/reference/androidx/browser/customtabs/CustomTabsCallback) and [engagement signals](https://developer.chrome.com/docs/android/custom-tabs/guide-engagement-signals/). Session specific signals are
   designed to avoid malicious actors inferring details about the content or the
   state of the web page. As such, engagement signals are disabled in some
   circumstances, such as when pages are opened using [text fragments](https://web.dev/text-fragments/#text-fragments).

2. **Current page URL can be shared with the embedder with explicit user action.**
   When a user taps on an embedding app action in CCT, the embedding application
   can see the full URL and origin of the currently visited page. In some instances,
   verifiable Google app entities can access the current page URL without user
   intent.

3. **Developers can send and receive messages as if they were a website which they
   can prove they control.** The postMessage API can be used by developers to
   establish a 2-way communication channel between the main frame inside the
   Chrome Custom Tab. For non-verifiable Google entities, this functionality is
   only supported if a [Digital Asset Link](https://developers.google.com/digital-asset-links)
   relationship has been established between a website and the embedding app.
   The website is then used as the origin
   for the [`window.postMessage()`](https://developer.mozilla.org/en-US/docs/Web/API/Window/postMessage)
   Web API, which enables cross-origin communication.

## How else might an embedder appear to interact with web content?

1. **The app may be able to draw over parts of the Chrome browser UI or the website.**
   Unlike the Chrome browser app which is always displayed in its own Android
   Task, Custom Tabs are most commonly displayed in the same Android Task as the
   embedding app. This makes Custom Tabs susceptible to certain tap jacking and
   phishing attacks. For example, a malicious actor could launch an
   Activity positioned over the web content or CT toolbar and draw UI to steal a
   password. The presence of pre-existing browser state and cookies may make the
   embedded web experience appear more trustworthy and therefore increase the
   likelihood of the phishing attack succeeding. Note that Android has been
   pursuing protections within the OS to mitigate against some attacks, and Chrome will
   continue to work with Android to protect users on older OS versions.

2. **Developers can add app specific actions into CCT**. Chrome provides customization
   options to embedding apps. The appearance of the bottom toolbar and its
   contents can be customized and can change during runtime. While this UI surface
   could be used for malicious purposes, we accept this risk because, overall, CCT
   has better security properties than WebView, and a high level of UI
   customisability is necessary to drive Custom Tab adoption. Furthermore, the
   space that can be occupied by the bottom toolbar is limited and the position is
   fixed, lowering the risk that users will fall for attacks launched from this
   surface.

## What data does an embedder not have access to?

**Embedders cannot access data unrelated to the CCT session**. This includes:

* history from past sessions
* cookies
* passwords
* full DOM access
* arbitrary script injection
* network request interception
* etc.

Any future access would require explicit permissions to be accepted.
