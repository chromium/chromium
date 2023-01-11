// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_event_listeners.h"

#include <functional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/listener_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

using APIEventListenersTest = APIBindingTest;
using MockEventChangeHandler = ::testing::StrictMock<
    base::MockCallback<APIEventListeners::ListenersUpdated>>;

const char kFunction[] = "(function() {})";
const char kEvent[] = "event";
const char kContextOwner[] = "context";

APIEventListeners::ContextOwnerIdGetter CreateContextOwnerIdGetter() {
  return base::BindRepeating(
      [](v8::Local<v8::Context>) { return std::string(kContextOwner); });
}

}  // namespace

// Test unfiltered listeners.
TEST_F(APIEventListenersTest, UnfilteredListeners) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  MockEventChangeHandler handler;
  ListenerTracker tracker;
  UnfilteredEventListeners listeners(handler.Get(), kEvent,
                                     CreateContextOwnerIdGetter(),
                                     binding::kNoListenerMax, true, &tracker);

  // Starting out, there should be no listeners.
  v8::Local<v8::Function> function_a = FunctionFromString(context, kFunction);
  EXPECT_EQ(0u, listeners.GetNumListeners());
  EXPECT_FALSE(listeners.HasListener(function_a));

  std::string error;
  v8::Local<v8::Object> filter;

  // Adding a new listener should trigger the callback (0 -> 1).
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kFirstUnfilteredListenerForContextOwnerAdded,
                           nullptr, true, context));
  EXPECT_TRUE(listeners.AddListener(function_a, filter, context, &error));
  ::testing::Mock::VerifyAndClearExpectations(&handler);

  // function_a should be registered as a listener, and should be returned when
  // we get the listeners.
  EXPECT_TRUE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());
  EXPECT_THAT(listeners.GetListeners(nullptr, context),
              testing::UnorderedElementsAre(function_a));

  // Trying to add function_a again should have no effect.
  EXPECT_FALSE(listeners.AddListener(function_a, filter, context, &error));
  EXPECT_TRUE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());

  v8::Local<v8::Function> function_b = FunctionFromString(context, kFunction);

  // We should not yet have function_b registered, and trying to remove it
  // should have no effect.
  EXPECT_FALSE(listeners.HasListener(function_b));
  listeners.RemoveListener(function_b, context);
  EXPECT_EQ(1u, listeners.GetNumListeners());
  EXPECT_THAT(listeners.GetListeners(nullptr, context),
              testing::UnorderedElementsAre(function_a));

  // Add function_b; there should now be two listeners, and both should be
  // returned when we get the listeners. However, the callback shouldn't be
  // triggered, since this isn't a 0 -> 1 or 1 -> 0 transition.
  EXPECT_TRUE(listeners.AddListener(function_b, filter, context, &error));
  EXPECT_TRUE(listeners.HasListener(function_b));
  EXPECT_EQ(2u, listeners.GetNumListeners());
  EXPECT_THAT(listeners.GetListeners(nullptr, context),
              testing::UnorderedElementsAre(function_a, function_b));

  // Remove function_a; there should now be only one listener. The callback
  // shouldn't be triggered.
  listeners.RemoveListener(function_a, context);
  EXPECT_FALSE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());
  EXPECT_THAT(listeners.GetListeners(nullptr, context),
              testing::UnorderedElementsAre(function_b));

  // Remove function_b (the final listener). No more listeners should remain.
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kLastUnfilteredListenerForContextOwnerRemoved,
                           nullptr, true, context));
  listeners.RemoveListener(function_b, context);
  ::testing::Mock::VerifyAndClearExpectations(&handler);
  EXPECT_FALSE(listeners.HasListener(function_b));
  EXPECT_EQ(0u, listeners.GetNumListeners());
  EXPECT_TRUE(listeners.GetListeners(nullptr, context).empty());
}

// Tests the invalidation of unfiltered listeners.
TEST_F(APIEventListenersTest, UnfilteredListenersInvalidation) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  MockEventChangeHandler handler;
  ListenerTracker tracker;
  UnfilteredEventListeners listeners(handler.Get(), kEvent,
                                     CreateContextOwnerIdGetter(),
                                     binding::kNoListenerMax, true, &tracker);

  listeners.Invalidate(context);

  v8::Local<v8::Function> function_a = FunctionFromString(context, kFunction);
  v8::Local<v8::Function> function_b = FunctionFromString(context, kFunction);
  std::string error;
  v8::Local<v8::Object> filter;
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kFirstUnfilteredListenerForContextOwnerAdded,
                           nullptr, true, context));
  EXPECT_TRUE(listeners.AddListener(function_a, filter, context, &error));
  ::testing::Mock::VerifyAndClearExpectations(&handler);
  EXPECT_TRUE(listeners.AddListener(function_b, filter, context, &error));

  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kLastUnfilteredListenerForContextOwnerRemoved,
                           nullptr, false, context));
  listeners.Invalidate(context);
  ::testing::Mock::VerifyAndClearExpectations(&handler);

  EXPECT_EQ(0u, listeners.GetNumListeners());
}

// Tests that unfiltered listeners ignore the filtering info.
TEST_F(APIEventListenersTest, UnfilteredListenersIgnoreFilteringInfo) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ListenerTracker tracker;
  UnfilteredEventListeners listeners(base::DoNothing(), kEvent,
                                     CreateContextOwnerIdGetter(),
                                     binding::kNoListenerMax, true, &tracker);
  v8::Local<v8::Function> function = FunctionFromString(context, kFunction);
  std::string error;
  v8::Local<v8::Object> filter;
  EXPECT_TRUE(listeners.AddListener(function, filter, context, &error));
  mojom::EventFilteringInfoPtr filtering_info =
      mojom::EventFilteringInfo::New();
  filtering_info->url = GURL("http://example.com/foo");
  EXPECT_THAT(listeners.GetListeners(std::move(filtering_info), context),
              testing::UnorderedElementsAre(function));
}

TEST_F(APIEventListenersTest, UnfilteredListenersMaxListenersTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ListenerTracker tracker;
  UnfilteredEventListeners listeners(base::DoNothing(), kEvent,
                                     CreateContextOwnerIdGetter(), 1, true,
                                     &tracker);

  v8::Local<v8::Function> function_a = FunctionFromString(context, kFunction);
  EXPECT_EQ(0u, listeners.GetNumListeners());

  std::string error;
  v8::Local<v8::Object> filter;
  EXPECT_TRUE(listeners.AddListener(function_a, filter, context, &error));
  EXPECT_TRUE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());

  v8::Local<v8::Function> function_b = FunctionFromString(context, kFunction);
  EXPECT_FALSE(listeners.AddListener(function_b, filter, context, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(listeners.HasListener(function_b));
  EXPECT_TRUE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());
}

TEST_F(APIEventListenersTest, UnfilteredListenersLazyListeners) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ListenerTracker tracker;
  MockEventChangeHandler handler;
  UnfilteredEventListeners listeners(handler.Get(), kEvent,
                                     CreateContextOwnerIdGetter(),
                                     binding::kNoListenerMax, false, &tracker);

  v8::Local<v8::Function> listener = FunctionFromString(context, kFunction);
  std::string error;
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kFirstUnfilteredListenerForContextOwnerAdded,
                           nullptr, false, context));
  listeners.AddListener(listener, v8::Local<v8::Object>(), context, &error);
  ::testing::Mock::VerifyAndClearExpectations(&handler);

  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kLastUnfilteredListenerForContextOwnerRemoved,
                           nullptr, false, context));
  listeners.RemoveListener(listener, context);
  ::testing::Mock::VerifyAndClearExpectations(&handler);
}

// Tests filtered listeners.
TEST_F(APIEventListenersTest, FilteredListeners) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  MockEventChangeHandler handler;
  ListenerTracker tracker;
  FilteredEventListeners listeners(handler.Get(), kEvent,
                                   CreateContextOwnerIdGetter(),
                                   binding::kNoListenerMax, true, &tracker);

  // Starting out, there should be no listeners registered.
  v8::Local<v8::Function> function_a = FunctionFromString(context, kFunction);
  EXPECT_EQ(0u, listeners.GetNumListeners());
  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kEvent));
  EXPECT_FALSE(listeners.HasListener(function_a));

  v8::Local<v8::Object> empty_filter;
  std::string error;

  const base::Value::Dict empty_dict;
  // Register function_a with no filter; this is equivalent to registering for
  // all events. The callback should be triggered since this is a 0 -> 1
  // transition.
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kFirstListenerWithFilterForContextOwnerAdded,
                           testing::Pointee(testing::Eq(std::cref(empty_dict))),
                           true, context));
  EXPECT_TRUE(listeners.AddListener(function_a, empty_filter, context, &error));
  ::testing::Mock::VerifyAndClearExpectations(&handler);

  // function_a should be registered, and should be returned when we get the
  // listeners.
  EXPECT_TRUE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());
  EXPECT_THAT(listeners.GetListeners(nullptr, context),
              testing::UnorderedElementsAre(function_a));

  // It should also be registered in the event filter.
  EXPECT_EQ(
      1, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kEvent));

  // Since function_a has no filter, associating a specific url should still
  // return function_a.
  mojom::EventFilteringInfo filtering_info_match;
  filtering_info_match.url = GURL("http://example.com/foo");
  EXPECT_THAT(listeners.GetListeners(filtering_info_match.Clone(), context),
              testing::UnorderedElementsAre(function_a));

  // Trying to add function_a again should have no effect.
  EXPECT_FALSE(
      listeners.AddListener(function_a, empty_filter, context, &error));
  EXPECT_TRUE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());

  v8::Local<v8::Function> function_b = FunctionFromString(context, kFunction);

  // function_b should not yet be registered, and trying to remove it should
  // have no effect.
  EXPECT_FALSE(listeners.HasListener(function_b));
  listeners.RemoveListener(function_b, context);
  EXPECT_EQ(1u, listeners.GetNumListeners());
  EXPECT_THAT(listeners.GetListeners(nullptr, context),
              testing::UnorderedElementsAre(function_a));

  // Register function_b with a filter for pathContains: 'foo'. Unlike
  // unfiltered listeners, this *should* trigger the callback, since there is
  // no other listener registered with this same filter.
  const base::Value::Dict expected_dict =
      base::test::ParseJsonDict(R"({"url": [{"pathContains": "foo"}]})");
  v8::Local<v8::Object> path_filter;
  {
    v8::Local<v8::Value> val =
        V8ValueFromScriptSource(context, "({url: [{pathContains: 'foo'}]})");
    ASSERT_TRUE(val->IsObject());
    path_filter = val.As<v8::Object>();
  }
  EXPECT_CALL(handler,
              Run(kEvent,
                  binding::EventListenersChanged::
                      kFirstListenerWithFilterForContextOwnerAdded,
                  testing::Pointee(testing::Eq(std::cref(expected_dict))), true,
                  context));
  EXPECT_TRUE(listeners.AddListener(function_b, path_filter, context, &error));
  ::testing::Mock::VerifyAndClearExpectations(&handler);

  // function_b should be present.
  EXPECT_TRUE(listeners.HasListener(function_b));
  EXPECT_EQ(2u, listeners.GetNumListeners());
  EXPECT_EQ(
      2, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kEvent));

  // function_b should ignore calls that don't specify an url, since they, by
  // definition, don't match.
  EXPECT_THAT(listeners.GetListeners(nullptr, context),
              testing::UnorderedElementsAre(function_a));
  // function_b should be included for matching urls...
  EXPECT_THAT(listeners.GetListeners(filtering_info_match.Clone(), context),
              testing::UnorderedElementsAre(function_a, function_b));
  // ... but not urls that don't match.
  mojom::EventFilteringInfo filtering_info_no_match;
  filtering_info_no_match.url = GURL("http://example.com/bar");
  EXPECT_THAT(listeners.GetListeners(filtering_info_no_match.Clone(), context),
              testing::UnorderedElementsAre(function_a));

  // Remove function_a. Since filtered listeners notify whenever there's a
  // change in listeners registered with a specific filter, this should trigger
  // the callback.
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kLastListenerWithFilterForContextOwnerRemoved,
                           testing::Pointee(testing::Eq(std::cref(empty_dict))),
                           true, context));
  listeners.RemoveListener(function_a, context);
  ::testing::Mock::VerifyAndClearExpectations(&handler);
  EXPECT_FALSE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());
  EXPECT_EQ(
      1, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kEvent));
  // function_b should be the only listener remaining, so we shouldn't find
  // any listeners for events without matching filters.
  EXPECT_TRUE(listeners.GetListeners(nullptr, context).empty());
  EXPECT_THAT(listeners.GetListeners(filtering_info_match.Clone(), context),
              testing::UnorderedElementsAre(function_b));
  EXPECT_TRUE(
      listeners.GetListeners(filtering_info_no_match.Clone(), context).empty());

  // Remove function_b. No listeners should remain.
  EXPECT_CALL(handler,
              Run(kEvent,
                  binding::EventListenersChanged::
                      kLastListenerWithFilterForContextOwnerRemoved,
                  testing::Pointee(testing::Eq(std::cref(expected_dict))), true,
                  context));
  listeners.RemoveListener(function_b, context);
  ::testing::Mock::VerifyAndClearExpectations(&handler);
  EXPECT_FALSE(listeners.HasListener(function_b));
  EXPECT_EQ(0u, listeners.GetNumListeners());
  EXPECT_TRUE(listeners.GetListeners(nullptr, context).empty());
  EXPECT_TRUE(
      listeners.GetListeners(filtering_info_match.Clone(), context).empty());
  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kEvent));
}

// Tests that adding multiple listeners with the same filter doesn't trigger
// the update callback.
TEST_F(APIEventListenersTest,
       UnfilteredListenersWithSameFilterDontTriggerUpdate) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  MockEventChangeHandler handler;
  ListenerTracker tracker;
  FilteredEventListeners listeners(handler.Get(), kEvent,
                                   CreateContextOwnerIdGetter(),
                                   binding::kNoListenerMax, true, &tracker);

  auto get_filter = [context]() {
    return V8ValueFromScriptSource(context, "({url: [{pathContains: 'foo'}]})")
        .As<v8::Object>();
  };

  v8::Local<v8::Function> function_a = FunctionFromString(context, kFunction);

  std::string error;
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kFirstListenerWithFilterForContextOwnerAdded,
                           testing::NotNull(), true, context));
  EXPECT_TRUE(listeners.AddListener(function_a, get_filter(), context, &error));
  ::testing::Mock::VerifyAndClearExpectations(&handler);
  EXPECT_EQ(
      1, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kEvent));

  v8::Local<v8::Function> function_b = FunctionFromString(context, kFunction);
  v8::Local<v8::Function> function_c = FunctionFromString(context, kFunction);
  EXPECT_TRUE(listeners.AddListener(function_b, get_filter(), context, &error));
  EXPECT_TRUE(listeners.AddListener(function_c, get_filter(), context, &error));
  EXPECT_EQ(3u, listeners.GetNumListeners());
  EXPECT_EQ(
      3, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kEvent));

  mojom::EventFilteringInfoPtr filtering_info_match =
      mojom::EventFilteringInfo::New();
  filtering_info_match->url = GURL("http://example.com/foo");
  EXPECT_THAT(
      listeners.GetListeners(std::move(filtering_info_match), context),
      testing::UnorderedElementsAre(function_a, function_b, function_c));

  listeners.RemoveListener(function_c, context);
  listeners.RemoveListener(function_b, context);

  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kLastListenerWithFilterForContextOwnerRemoved,
                           testing::NotNull(), true, context));
  listeners.RemoveListener(function_a, context);
  ::testing::Mock::VerifyAndClearExpectations(&handler);
  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kEvent));
}

// Tests that trying to add a listener with an invalid filter fails.
TEST_F(APIEventListenersTest, UnfilteredListenersError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ListenerTracker tracker;
  FilteredEventListeners listeners(base::DoNothing(), kEvent,
                                   CreateContextOwnerIdGetter(),
                                   binding::kNoListenerMax, true, &tracker);

  v8::Local<v8::Object> invalid_filter =
      V8ValueFromScriptSource(context, "({url: 'some string'})")
          .As<v8::Object>();
  v8::Local<v8::Function> function = FunctionFromString(context, kFunction);
  std::string error;
  EXPECT_FALSE(
      listeners.AddListener(function, invalid_filter, context, &error));
  EXPECT_FALSE(error.empty());
}

// Tests that adding listeners for multiple different events is correctly
// recorded in the EventFilter.
TEST_F(APIEventListenersTest, MultipleUnfilteredListenerEvents) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  const char kAlpha[] = "alpha";
  const char kBeta[] = "beta";

  ListenerTracker tracker;
  FilteredEventListeners listeners_a(base::DoNothing(), kAlpha,
                                     CreateContextOwnerIdGetter(),
                                     binding::kNoListenerMax, true, &tracker);
  FilteredEventListeners listeners_b(base::DoNothing(), kBeta,
                                     CreateContextOwnerIdGetter(),
                                     binding::kNoListenerMax, true, &tracker);

  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kAlpha));
  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kBeta));

  std::string error;
  v8::Local<v8::Object> filter;

  v8::Local<v8::Function> function_a = FunctionFromString(context, kFunction);
  EXPECT_TRUE(listeners_a.AddListener(function_a, filter, context, &error));
  EXPECT_EQ(
      1, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kAlpha));
  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kBeta));

  v8::Local<v8::Function> function_b = FunctionFromString(context, kFunction);
  EXPECT_TRUE(listeners_b.AddListener(function_b, filter, context, &error));
  EXPECT_EQ(
      1, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kAlpha));
  EXPECT_EQ(
      1, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kBeta));

  listeners_b.RemoveListener(function_b, context);
  EXPECT_EQ(
      1, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kAlpha));
  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kBeta));

  listeners_a.RemoveListener(function_a, context);
  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kAlpha));
  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kBeta));
}

// Tests the invalidation of filtered listeners.
TEST_F(APIEventListenersTest, FilteredListenersInvalidation) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  MockEventChangeHandler handler;
  ListenerTracker tracker;
  FilteredEventListeners listeners(handler.Get(), kEvent,
                                   CreateContextOwnerIdGetter(),
                                   binding::kNoListenerMax, true, &tracker);
  listeners.Invalidate(context);

  v8::Local<v8::Object> empty_filter;
  v8::Local<v8::Object> filter =
      V8ValueFromScriptSource(context, "({url: [{pathContains: 'foo'}]})")
          .As<v8::Object>();
  std::string error;

  v8::Local<v8::Function> function_a = FunctionFromString(context, kFunction);
  v8::Local<v8::Function> function_b = FunctionFromString(context, kFunction);
  v8::Local<v8::Function> function_c = FunctionFromString(context, kFunction);

  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kFirstListenerWithFilterForContextOwnerAdded,
                           testing::NotNull(), true, context));
  EXPECT_TRUE(listeners.AddListener(function_a, empty_filter, context, &error));
  ::testing::Mock::VerifyAndClearExpectations(&handler);
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kFirstListenerWithFilterForContextOwnerAdded,
                           testing::NotNull(), true, context));
  EXPECT_TRUE(listeners.AddListener(function_b, filter, context, &error));
  ::testing::Mock::VerifyAndClearExpectations(&handler);
  EXPECT_TRUE(listeners.AddListener(function_c, filter, context, &error));

  // Since two listener filters are present in the list, we should be notified
  // of each going away when we invalidate the context.
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kLastListenerWithFilterForContextOwnerRemoved,
                           testing::NotNull(), false, context))
      .Times(2);
  listeners.Invalidate(context);
  ::testing::Mock::VerifyAndClearExpectations(&handler);

  EXPECT_EQ(0u, listeners.GetNumListeners());
  EXPECT_EQ(
      0, tracker.event_filter_for_testing()->GetMatcherCountForEventForTesting(
             kEvent));
}

TEST_F(APIEventListenersTest, FilteredListenersMaxListenersTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ListenerTracker tracker;
  FilteredEventListeners listeners(base::DoNothing(), kEvent,
                                   CreateContextOwnerIdGetter(), 1, true,
                                   &tracker);

  v8::Local<v8::Function> function_a = FunctionFromString(context, kFunction);
  EXPECT_EQ(0u, listeners.GetNumListeners());

  std::string error;
  v8::Local<v8::Object> filter;
  EXPECT_TRUE(listeners.AddListener(function_a, filter, context, &error));
  EXPECT_TRUE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());

  v8::Local<v8::Function> function_b = FunctionFromString(context, kFunction);
  EXPECT_FALSE(listeners.AddListener(function_b, filter, context, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(listeners.HasListener(function_b));
  EXPECT_TRUE(listeners.HasListener(function_a));
  EXPECT_EQ(1u, listeners.GetNumListeners());
}

TEST_F(APIEventListenersTest, FilteredListenersLazyListeners) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  MockEventChangeHandler handler;
  ListenerTracker tracker;
  FilteredEventListeners listeners(handler.Get(), kEvent,
                                   CreateContextOwnerIdGetter(),
                                   binding::kNoListenerMax, false, &tracker);

  v8::Local<v8::Function> listener = FunctionFromString(context, kFunction);
  std::string error;
  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kFirstListenerWithFilterForContextOwnerAdded,
                           testing::NotNull(), false, context));
  listeners.AddListener(listener, v8::Local<v8::Object>(), context, &error);
  ::testing::Mock::VerifyAndClearExpectations(&handler);

  EXPECT_CALL(handler, Run(kEvent,
                           binding::EventListenersChanged::
                               kLastListenerWithFilterForContextOwnerRemoved,
                           testing::NotNull(), false, context));
  listeners.RemoveListener(listener, context);
  ::testing::Mock::VerifyAndClearExpectations(&handler);
}

}  // namespace extensions
