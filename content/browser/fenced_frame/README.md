# Fenced Frame: Guidelines for Feature Behavior

Fenced frames are a new HTML element that allows a page to be embedded in
another page while preventing any information from being exchanged between the
two pages. To maintain this “fence” between a fenced frame and its embedder, any
new features need to consider how they should behave inside a fenced frame.
Specifically, should they treat the fenced frame as the top-most frame? Or
should the feature be allowed to traverse past the fenced frame to get
information about its embedder?


### Decision chart

The decision chart below is a basic guide to determine how a fenced frame should
behave when using a feature:

![drawing](https://docs.google.com/drawings/d/1BFcncZZVogJXnhccqKnE9y23yf42f_AMamwVpGSlLlU/export/png)

Below are more details for each node:


### Does this feature need to access the outer frame tree to work properly?

Some features need to interact with either a frame’s immediate ancestor, the
nearest main frame in the frame tree, or the outermost main frame in the frame
tree.

*   User activation needs to have access to a frame’s ancestors (and their
    ancestors) to give all of them user activation. However, the feature will
    not break if it can’t cross the fenced frame boundary to a frame in the
    outer frame tree.
*   `window.top` needs to get access to the nearest main frame in the frame tree
    (the top-most frame as far as the context is concerned). Even though the
    behavior will be different than if it were to get the outermost main frame,
    it does not fundamentally break the functionality of this feature.
*   Accessibility trees use the outermost main frame as the root to build the
    entire accessibility tree from. If a frame in a fenced frame tree doesn’t
    have access to the outermost main frame, then the accessibility tree won’t
    be built properly and accessibility functionality will break.

If your feature only needs the immediate ancestor or the nearest main frame,
then fenced frames should **act as a main frame** with your feature..

If you determine that your feature needs a fenced frame to access the outer
frame tree (i.e. **act as an iframe**), you must ensure that no information can
leak across the fenced boundary. If any information can leak across the
boundary, the feature must be disabled. If the feature cannot be completely
disabled, a new approach might need to be formulated (e.g. the ongoing work
regarding intersection observer API).


### Will it be possible for the web platform to access this information?

As a direct result of your feature, would you be able to write some JavaScript
code whose output changes based on the information your feature knows about
ancestors in the embedder frame tree? If you decide that your feature needs to
give a fenced frame access to information about its ancestors, this next check
is very important. You should ensure that the information gathered from an outer
frame tree does not end up in a place where it can be observed on the web
platform (i.e. through JavaScript). If it is possible for information to cross
the fenced frame boundary in a web-observable way, and there’s no way to patch
it, the feature must be disabled in fenced frames.

If you can guarantee that your information is
[k-anonymized](https://github.com/WICG/turtledove/blob/main/FLEDGE_k_anonymity_server.md)
through something like
[FLEDGE](https://github.com/WICG/turtledove/blob/main/FLEDGE.md), or scrubbed
via something like link decoration mitigation, even though information is
flowing across the fenced frame boundary, it might be anonymized enough to allow
the feature.


### Act as main frame.

Unless the feature needs access to something outside of a fenced frame, this is
how fenced frames will most likely act with your feature. This means that the
feature will think that the fenced frame root is the root of the entire frame
tree. This also means that accidentally leaking data across the fenced frame
boundary is a lot less likely.

This is accomplished by calling helper functions like
`RenderFrameHostImpl::GetParent()` and `RenderFrameHostImpl::GetMainFrame()`.

*   In `RenderFrameHostImpl::SetWindowRect`, there’s a check to make sure the
    call came from the outermost document. Because fenced frames are meant to
    create a boundary and be their own root frame, having the fenced frame **act
    as a main frame** is the correct behavior here. It does not need to know
    that there is another frame above it, since it should be acting as if it
    were its own separate tab.
*   The `window.top` JavaScript call, if it could reach beyond the fenced frame
    root, could allow the fenced frame to learn about its embedder. Having
    `window.top` stop at the fenced frame root will not completely break the
    feature, since it will just be acting as if the fenced frame were its own
    separate tab. Because of that, the fenced frame should **act as the main
    frame** for this call.
    *   The same logic applies for other calls like `window.parent`,
        `window.postMessage`, and `history.length`.


### Act as iframe.

This feature will be made aware of frames above the fenced frame root, and know
that there is an outermost root that is not within the fenced frame tree. This
path requires extra care, since it becomes possible to have a corner case
accidentally leak data across the fenced frame boundary.

This is accomplished by calling helper functions like
`RenderFrameHostImpl::GetParentOrOuterDocument()` and
`RenderFrameHostImpl::GetOutermostMainFrame()`.

*   The accessibility tree feature assumes that there is one root frame that
    everything else branches off of. This is needed to build the accessibility
    tree, which must have only one root and be able to see everything. That
    information is exposed to screen readers and other accessibility features,
    but is never given directly to the web platform. In this case, it is okay
    for the fenced frame to **act as an iframe**.
*   When passing focus between frames after the user tab-focuses (switches focus
    by hitting the tab key), a child fenced frame needs to know what its parent
    is in order to pass focus off to it. If it’s not allowed to know, it will
    only be able to pass focus to child frames. The web platform never learns
    who sent focus to it. That information stays solely in the browser, and all
    the web platform learns is that it now has focus. Because of that, it is
    okay for the fenced frame to **act as an iframe**.
*   Prerendering uses the root frame tree node’s ID as the key to find the
    pre-rendered page. If it attempted to pass in the ID of the fenced frame
    node instead, it would not be able to find the pre-rendered page. In this
    case, the fenced frame would need to **act as an iframe**.
*   Extensions are a special case. By default, they have access to everything in
    every tab. For ad blocking extensions specifically, they need to be able to
    see inside of a fenced frame to know whether the URL being loaded is an ad
    or not (not breaking content blockers is an invariant of the fenced frame
    design). Therefore, a fenced frame needs to **act as an iframe** so the ad
    blocker can cross the fence and work as expected. [See the integration with
    extensions explainer document for more
    details](https://github.com/WICG/fenced-frame/blob/master/explainer/integration_with_web_platform.md#extensions).


### Disable in fenced frames.

If a feature needs to know about something outside of a fenced frame to function
properly, but it’s impossible to expose that information without introducing a
leak, the only option is to disallow the feature inside a fenced frame entirely.

*   [Permissions policies](https://www.w3.org/TR/permissions-policy-1/) inherit
    from a frame’s parent. For cases where the url loaded in the fenced frame is
    not set by the embedder, we need to make sure other ways of communicating
    information are restricted. By delegating permissions, a fenced frame is
    allowed to learn what its parent’s permissions policy is (either through
    their headers or through the allow=”” attribute in the frame object), which
    opens the door to fingerprinting. Tests can easily be written in JavaScript
    to determine whether a policy is enabled or disabled. So, to prevent that,
    inheritance needs to be **disabled in a fenced frame**.
    *   With the opaque ads configuration design, setting permissions policies
        [will be allowed in a fenced
        frame](https://docs.google.com/document/d/11QaI40IAr12CDFrIUQbugxmS9LfircghHUghW-EDzMk/edit#heading=h.1mqtrutx4yv4).
        However, the actual policies will be checked by a [k-anonymity
        check](https://github.com/WICG/turtledove/blob/main/FLEDGE_k_anonymity_server.md),
        and the URL will not win the FLEDGE auction and not load in the frame if
        it fails the check. This has the effect of decoupling this information
        from the parent frame. While the fenced frame will learn about the
        permissions it was allowed to be created in, it won’t know which ones
        came from the page and which ones came from FLEDGE, and they are
        guaranteed to be k-anonymous enough for fingerprinting to not work.
*   Modifying parameters in a fenced frame (such as the dimensions of the frame
    or its mode) is an easy way for an embedding page to pass information
    through the fence. Modifying parameters requires information to be passed
    from an embedding page into the fenced frame, but as soon as a parameter is
    modified, the fenced frame can easily access it. So, modifying parameters
    needs to be **disabled in a fenced frame**.
    *   In the case of resizing a fenced frame, we do allow the resizing call to
        go through. However, this only resizes the outer frame in a fenced
        frame. The inner frame retains its original dimensions, but is visually
        scaled to fit in the new outer frame. As far as the page embedded in the
        fenced frame is concerned, its dimensions did not change.
*   Navigating a fenced frame to a URL like a “javascript:” url could allow
    arbitrary data to flow across the fence. So, this is **disabled in a fenced
    frame**.


## Writing Tests

There are 2 instances where you will need to write tests to ensure your feature
works as expected with fenced frames:

*   If it is possible for the **web platform to gain access to information**
    from your feature (regardless of whether you **disable the feature** or have
    fenced frames **act as a main frame**), then you will need to write a test
    to make sure that the information can’t be leaked across a fenced frame
    boundary.
    *   If you are disabling your feature, the test can be as simple as making
        sure the feature doesn’t work inside a fenced frame.
    *   If you are having your feature **act as a main frame**, the test should
        ensure that your feature is not introducing information to a fenced
        frame that changes if the conditions of the embedding frame change.
        *   For example, `window.top` works inside a fenced frame, but it just
            returns the fenced frame root. To ensure that this is working
            correctly and not actually returning the outermost main frame, there
            should be [a test confirming this
            behavior](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/wpt_internal/fenced_frame/window-top.https.html).
*   If you are having a fenced frame act as an iframe for your feature, you will
    need to write a test to ensure that the feature is working properly inside a
    fenced frame. This test can be as simple as triggering the feature in a
    fenced frame and verifying the output is what you expect it to be.

There is already infrastructure set up to help you write fenced frames tests.

*   For **web platform tests**, there is a [directory for fenced frame
    tests](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/wpt_internal/fenced_frame/).
    In it, you’ll find [a utils file with helper
    functions](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/wpt_internal/fenced_frame/resources/utils.js)
    to speed up the test writing process, as well as other tests you can use as
    inspiration.
*   For **browser tests**, there is a [fenced frame test helper
    file](https://source.chromium.org/chromium/chromium/src/+/main:content/public/test/fenced_frame_test_util.h)
    that serves the same purpose. For inspiration, the
    [fenced\_frame\_browsertest.cc](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/fenced_frame/fenced_frame_browsertest.cc)
    file has a lot of tests that can be used as a starting point. There are
    [fenced frame tests in other
    files](https://source.chromium.org/search?q=fencedframe%20file:test.cc$&start=1)
    that you can also use.
*   For **unit tests**, the `RenderFrameHostTester` class has an
    [AppendFencedFrame](https://source.chromium.org/chromium/chromium/src/+/main:content/public/test/test_renderer_host.h;l=153-156;drc=aaea55c708c63d53a89fb525484aa94747599714)
    function. However, double check the file you’re adding a test to, since it
    might already have [helper functions in its testing
    class](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/media/media_engagement_contents_observer_unittest.cc;l=1423;drc=c94a0d209dee1da75c4131360b75702a8245dd5c)
    to create fenced frames.
