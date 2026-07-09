// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/web_request_natives.h"

#include <string>

#include "extensions/renderer/module_system.h"
#include "extensions/renderer/module_system_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class WebRequestNativesTest : public ModuleSystemTest {
 protected:
  void RunTest(const std::string& js) {
    ModuleSystem::NativesEnabledScope natives_enabled(env()->module_system());
    env()->module_system()->RegisterNativeHandler(
        "web_request_natives",
        std::make_unique<WebRequestNatives>(env()->context()));
    env()->RegisterModule(
        "test",
        std::string("var natives = requireNative('web_request_natives');\n"
                    "var assert = requireNative('assert');\n") +
            js);
    env()->module_system()->Require("test");
  }
};

// A tracked listener matches a URL its filter covers and not one it doesn't.
TEST_F(WebRequestNativesTest, MatchesAndDoesNotMatchByUrl) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 5,
        {urls: ['http://example.com/*']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);

    // A URL the filter covers is matched.
    var match = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(match.length === 1);
    assert.AssertTrue(match[0] === 5);

    // A URL it doesn't cover is not.
    var noMatch = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://other.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(noMatch.length === 0);
  )");
}

// After UntrackListener, the listener is no longer returned.
TEST_F(WebRequestNativesTest, UntrackRemovesListener) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 5,
        {urls: ['http://example.com/*']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);

    // It matches while tracked.
    var before = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(before.length === 1);

    // After untracking, it no longer matches.
    natives.UntrackListener(5);
    var after = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(after.length === 0);
  )");
}

// A filter the browser would reject (malformed URL pattern) fails to parse, so
// the listener is not tracked and never matches; untracking it is a no-op.
TEST_F(WebRequestNativesTest, InvalidFilterNeverMatches) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 1,
        {urls: ['invalid_url_pattern']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);

    var match = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(match.length === 0);

    // Untracking the never-tracked listener is a safe no-op.
    natives.UntrackListener(1);
  )");
}

// A blocking listener is excluded from a dispatch that does not want a response
// and included in one that does.
TEST_F(WebRequestNativesTest, BlockingExcludedUnlessResponseWanted) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 1,
        {urls: ['http://example.com/*']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/true, /*isAsyncBlocking=*/false);

    // Excluded when the dispatch does not want a response.
    var excluded = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(excluded.length === 0);

    // Included when it does.
    var included = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/true);
    assert.AssertTrue(included.length === 1);
    assert.AssertTrue(included[0] === 1);
  )");
}

// A listener bound to a webview instance is only returned for that instance.
TEST_F(WebRequestNativesTest, WebviewInstanceScoping) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 1,
        {urls: ['http://example.com/*']}, /*webViewInstanceId=*/7,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);

    // Not returned for a different webview instance.
    var wrongInstance = natives.GetMatchingListeners(
        'webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(wrongInstance.length === 0);

    // Returned for its own webview instance.
    var rightInstance = natives.GetMatchingListeners(
        'webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/7, /*wantsResponse=*/false);
    assert.AssertTrue(rightInstance.length === 1);
    assert.AssertTrue(rightInstance[0] === 1);
  )");
}

// Two listeners with identical filters keep distinct IDs; a matching dispatch
// returns both.
TEST_F(WebRequestNativesTest, IdenticalFiltersDistinctIds) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 1,
        {urls: ['http://example.com/*']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);
    natives.TrackListener('webRequest.onBeforeRequest', 2,
        {urls: ['http://example.com/*']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);

    // Both listeners are returned, with their distinct IDs.
    var match = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    match.sort(function(a, b) { return a - b; });
    assert.AssertTrue(match.length === 2);
    assert.AssertTrue(match[0] === 1);
    assert.AssertTrue(match[1] === 2);
  )");
}

// The registry records each listener's event: a sibling event's listener is
// never returned.
TEST_F(WebRequestNativesTest, CrossEventIsolation) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 1,
        {urls: ['http://example.com/*']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);
    natives.TrackListener('webRequest.onHeadersReceived', 2,
        {urls: ['http://example.com/*']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);

    // Querying one event ignores the other event's listener.
    var match = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(match.length === 1);
    assert.AssertTrue(match[0] === 1);
  )");
}

// A listener that filters on a resource type matches that type and not another.
TEST_F(WebRequestNativesTest, TypeFiltering) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 1,
        {urls: ['<all_urls>'], types: ['image']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);

    // Matches the filtered resource type.
    var imageMatch = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(imageMatch.length === 1);
    assert.AssertTrue(imageMatch[0] === 1);

    // Does not match a different resource type.
    var frameMatch = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'main_frame', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(frameMatch.length === 0);
  )");
}

// A request type the renderer cannot parse matches no listeners.
TEST_F(WebRequestNativesTest, InvalidRequestTypeMatchesNothing) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 1,
        {urls: ['<all_urls>']}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);

    var match = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'bogus_type', /*tabId=*/-1, /*windowId=*/-1,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(match.length === 0);
  )");
}

// A listener that filters on tabId/windowId matches only requests with those
// IDs.
TEST_F(WebRequestNativesTest, TabAndWindowIdFiltering) {
  RunTest(R"(
    natives.TrackListener('webRequest.onBeforeRequest', 1,
        {urls: ['<all_urls>'], tabId: 7, windowId: 9}, /*webViewInstanceId=*/0,
        /*isBlocking=*/false, /*isAsyncBlocking=*/false);

    // A different tab id does not match.
    var wrongTab = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/3, /*windowId=*/9,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(wrongTab.length === 0);

    // A different window id does not match.
    var wrongWindow = natives.GetMatchingListeners(
        'webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/7, /*windowId=*/4,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(wrongWindow.length === 0);

    // Both IDs match.
    var match = natives.GetMatchingListeners('webRequest.onBeforeRequest',
        'http://example.com/x', 'image', /*tabId=*/7, /*windowId=*/9,
        /*instanceId=*/0, /*wantsResponse=*/false);
    assert.AssertTrue(match.length === 1);
    assert.AssertTrue(match[0] === 1);
  )");
}

}  // namespace extensions
