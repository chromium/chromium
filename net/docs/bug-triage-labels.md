# Chrome Network Bug Triage : Components and labels

## Some network component caveats

* **Internals>Network>SSL**

    This includes issues that should be also tagged as **Security>UX**
    (certificate error pages or other security interstitials, omnibox indicators
    that a page is secure), and more general SSL issues.  If you see requests
    that die in the SSL negotiation phase, in particular, this is often the
    correct component.

* **Internals>Network>Cache**

    The cache is the layer that handles most range request logic (Though range
    requests may also be issued by the PDF plugin, XHRs, or other components).

* **Internals>Network>HTTP**

    Typically not used.  Unclear what it covers, and there's no specific HTTP
    owner.

* **Internals>Network>Logging**

    Covers **about:net-internals**, **about:net-export** as well as the what's
    sent to the NetLog.

* **Internals>Network>Connectivity**

    Issues related to switching between networks, `ERR_NETWORK_CHANGED`, Chrome
    thinking it's online when it's not / navigator.onLine inaccuracies, etc.

* **Internals>Network>Filters**

    Covers gzip, deflate and brotli issues.  `ERR_CONTENT_DECODING_FAILED`
    indicates a problem at this layer, and bugs here can also cause response
    body corruption.

## Common non-network components

Bugs in these areas often receive the **Internals>Network** component, though
they fall largely outside the purview of the network stack team:

* **UI>Browser>Downloads**

    Despite the name, this covers all issues related to downloading a file
    except saving entire pages (which is **Blink>SavePage**), not just UI
    issues.  Most downloads bugs will have the word "download" or "save as" in
    the description.  Issues with the HTTP server for the Chrome binaries are
    not downloads bugs.

* **Services>Safebrowsing**

    Bugs that have to do with the process by which a URL or file is determined
    to be dangerous based on our databases, or the resulting interstitials.
    Determination of danger based purely on content-type or file extension
    belongs in **UI>Browser>Downloads**, not SafeBrowsing.

* **Blink>Forms**

    Issues submitting forms, forms having weird data, forms sending the wrong
    method, etc.

* **Blink>Loader**

    Cross origin issues are sometimes loader related.  Blink also has an
    in-memory cache, and when it's used, requests don't appear in
    about:net-internals.  Requests for the same URL are also often merged there
    as well.  This does *not* cover issues with content/browser/loader/ files.

* **Blink>ServiceWorker**

* **Blink>Network>WebSockets**

    Issues with the WebSockets.  Attach this component to any issue about the
    WebSocket feature regardless of where the cause of the issue is (net/ or
    Blink).

* **Blink>Network>FetchAPI**

    Generic issues with the Fetch API - missing request or response headers,
    multiple headers, etc.  These will often run into issues in certain corner
    cases (Cross origin / CORS, proxy, whatever).  Attach all components that
    seem appropriate.

* **Blink>Network>XHR**

    Generic issues with sync/async XHR requests.

* **Blink>WebRTC>Network**

    Anything WebRTC-related does not use the net stack and should go here.

* **Services>Sync**

    Sharing data/tabs/history/passwords/etc between machines not working.

* **Services>Chromoting**

* **Platform>Extensions**

    Issues extensions loading / not loading / hanging.

* **Platform>Extensions>API**

    Issues with network related extension APIs should have this component.
    chrome.webRequest is the big one, I believe, but there are others.

* **Internals>Plugins>Pepper[>SDK]**

* **UI>Browser>Omnibox**

    Basically any issue with the omnibox.  URLs being treated as search queries
    rather than navigations, dropdown results being weird, not handling certain
    Unicode characters, etc.  If the issue is new TLDs not being recognized by
    the omnibox, that's due to Chrome's TLD list being out of date, and not an
    omnibox issue.  Such TLD issues should be duped against
    http://crbug.com/37436.

* **Internals>Media>Network**

    Issues related to media.  These often run into the 6 requests per hostname
    issue, and also have fun interactions with the cache, particularly in the
    range request case.

* **Internals>Plugins>PDF**

    Issues loading PDF files.  These are often related to range requests, which
    also have some logic at the Internals>Network>Cache layer.

* **UI>Browser>Navigation**

    Despite the name, this covers all issues related to page navigation, not
    just UI issues.

* **UI>Browser>History**

    Issues which only appear with forward/back navigation.

* **OS>Systems>Network** / **OS>Systems>Mobile** / **OS>Systems>Bluetooth**

    These should be used for issues with Chrome OS's platform network code, and
    not net/ issues on Chrome OS.

* **Blink>SecurityFeature**

    CORS / Cross origin issues.  Main frame cross-origin navigation issues are
    often actually **UI>Browser>Navigation** issues.

* **Privacy**

    Privacy related bug (History, cookies discoverable by an entity that
    shouldn't be able to do so, incognito state being saved in memory or on disk
    beyond the lifetime of incognito tabs, etc).  Generally used in conjunction
    with other components.

## Common labels

* **Type-Bug-Security**

    Security related bug (Allows for code execution from remote site, allows
    crossing security boundaries, unchecked array bounds, etc) should be tagged
    with this label.
