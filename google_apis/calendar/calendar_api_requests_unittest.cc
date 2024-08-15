// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_requests.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace calendar {

namespace {
const char kTestCalendarColorId[] = "5";
const char kTestPrimaryCalendarColorId[] = "14";
const char kTestPrimaryCalendarSummary[] = "test1@google.com";
const char kTestUserAgent[] = "test-user-agent";
}

class CalendarApiRequestsTest : public testing::Test {
 public:
  CalendarApiRequestsTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
                nullptr /* network_service */,
                true /* is_trusted */)) {}

  void SetUp() override {
    request_sender_ = std::make_unique<RequestSender>(
        std::make_unique<DummyAuthService>(), test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), kTestUserAgent,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    test_server_.RegisterRequestHandler(
        base::BindRepeating(&CalendarApiRequestsTest::HandleDataFileRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
    url_generator_ = std::make_unique<CalendarApiUrlGenerator>();
    url_generator_->SetBaseUrlForTesting(test_server_.base_url().spec());
  }

  void TearDown() override {
    // Deleting the sender here will delete all request objects.
    request_sender_.reset();
    // Wait for any DeleteSoon tasks to run.
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<RequestSender> request_sender_;
  std::unique_ptr<CalendarApiUrlGenerator> url_generator_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_;
  net::test_server::HttpRequest http_request_;

  // Returns a mock response based on the request URL.
  std::unique_ptr<net::test_server::HttpResponse> HandleDataFileRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    if (net::test_server::ShouldHandle(http_request_,
                                       "/calendar/v3/calendars")) {
      return test_util::CreateHttpResponseFromFile(
          test_util::GetTestFilePath("calendar/events.json"));
    }
    if (net::test_server::ShouldHandle(http_request_,
                                       "/calendar/v3/users/me/calendarList")) {
      return test_util::CreateHttpResponseFromFile(
          test_util::GetTestFilePath("calendar/calendar_list.json"));
    }
    NOTREACHED();
  }
};

// Checks that CalendarApiCalendarListRequest can generate the correct url
// and get a calendar list response.
TEST_F(CalendarApiRequestsTest, GetCalendarListRequest) {
  ApiErrorCode error = OTHER_ERROR;
  std::unique_ptr<CalendarList> calendars;

  {
    base::RunLoop run_loop;
    auto request = std::make_unique<CalendarApiCalendarListRequest>(
        request_sender_.get(), *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&error, &calendars)));

    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(
      "/calendar/v3/users/me/calendarList"
      "?maxResults=250"
      "&fields=etag%2Ckind%2Citems"
      "(kind%2Cid%2Csummary%2CcolorId%2Cselected%2Cprimary)",
      http_request_.relative_url);

  ASSERT_TRUE(calendars.get());
  EXPECT_EQ("calendar#calendarList", calendars->kind());
  EXPECT_EQ(kTestPrimaryCalendarSummary, calendars->items()[0]->summary());
  EXPECT_EQ(kTestPrimaryCalendarColorId, calendars->items()[0]->color_id());
  EXPECT_EQ(true, calendars->items()[0]->selected());
  EXPECT_EQ(true, calendars->items()[0]->primary());
}

// Tests that CalendarApiEventsRequest can generate the correct url and get the
// correct event list response.
TEST_F(CalendarApiRequestsTest, GetEventListRequest) {
  ApiErrorCode error = OTHER_ERROR;
  std::unique_ptr<EventList> events;
  base::Time start;
  base::Time end;

  EXPECT_TRUE(base::Time::FromString("13 Jun 2021 10:00 GMT", &start));
  EXPECT_TRUE(base::Time::FromString("16 Jun 2021 10:00 GMT", &end));

  {
    base::RunLoop run_loop;
    auto request = std::make_unique<CalendarApiEventsRequest>(
        request_sender_.get(), *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop, test_util::CreateCopyResultCallback(&error, &events)),
        start, end, /*calendar_id=*/"test1@google.com",
        /*calendar_color_id=*/kTestCalendarColorId);

    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(
      "/calendar/v3/calendars/test1@google.com/events"
      "?timeMin=2021-06-13T10%3A00%3A00.000Z"
      "&timeMax=2021-06-16T10%3A00%3A00.000Z"
      "&singleEvents=true"
      "&maxAttendees=1"
      "&maxResults=2500"
      "&fields=timeZone%2Cetag%2Ckind%2Citems(id%2Ckind%"
      "2Csummary%2CcolorId%2Cstatus%"
      "2Cstart(date)%2Cend(date)%"
      "2Cstart(dateTime)%2Cend(dateTime)%"
      "2ChtmlLink%2Cattendees(responseStatus%2Cself)%2CattendeesOmitted%"
      "2CconferenceData(conferenceId%2CentryPoints(entryPointType%2Curi))%"
      "2Ccreator(self)%2Clocation)",
      http_request_.relative_url);

  ASSERT_TRUE(events.get());

  EXPECT_EQ(events->time_zone(), "America/Los_Angeles");
  base::Time::Exploded exploded;
  events->items()[0]->start_time().date_time().LocalExplode(&exploded);
  EXPECT_EQ(exploded.month, 11);
  // Verifies that events containing a colorId do not have their color IDs
  // replaced by calendar_color_id.
  EXPECT_EQ(events->items()[0]->color_id(), "3");
  EXPECT_EQ(events->items()[1]->color_id(), "3");
  // Verifies that an event without a colorId in the response yields an event
  // object with a color ID equal to calendar_color_id (prepended by a marker).
  EXPECT_EQ(events->items()[2]->color_id(),
            calendar::kInjectedColorIdPrefix + kTestCalendarColorId);
}

// Tests that CalendarApiEventsRequest can generate the correct url and get the
// correct event list response with default optional parameters.
TEST_F(CalendarApiRequestsTest,
       GetEventListRequestWithDefaultOptionalParameters) {
  ApiErrorCode error = OTHER_ERROR;
  std::unique_ptr<EventList> events;
  base::Time start;
  base::Time end;

  EXPECT_TRUE(base::Time::FromString("13 Jun 2021 10:00 GMT", &start));
  EXPECT_TRUE(base::Time::FromString("16 Jun 2021 10:00 GMT", &end));

  {
    base::RunLoop run_loop;
    auto request = std::make_unique<CalendarApiEventsRequest>(
        request_sender_.get(), *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop, test_util::CreateCopyResultCallback(&error, &events)),
        start, end);

    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(
      "/calendar/v3/calendars/primary/events"
      "?timeMin=2021-06-13T10%3A00%3A00.000Z"
      "&timeMax=2021-06-16T10%3A00%3A00.000Z"
      "&singleEvents=true"
      "&maxAttendees=1"
      "&maxResults=2500"
      "&fields=timeZone%2Cetag%2Ckind%2Citems(id%2Ckind%"
      "2Csummary%2CcolorId%2Cstatus%"
      "2Cstart(date)%2Cend(date)%"
      "2Cstart(dateTime)%2Cend(dateTime)%"
      "2ChtmlLink%2Cattendees(responseStatus%2Cself)%2CattendeesOmitted%"
      "2CconferenceData(conferenceId%2CentryPoints(entryPointType%2Curi))%"
      "2Ccreator(self)%2Clocation)",
      http_request_.relative_url);

  ASSERT_TRUE(events.get());

  EXPECT_EQ(events->time_zone(), "America/Los_Angeles");
  base::Time::Exploded exploded;
  events->items()[0]->start_time().date_time().LocalExplode(&exploded);
  EXPECT_EQ(exploded.month, 11);
  EXPECT_EQ(events->items()[0]->color_id(), "3");
  EXPECT_EQ(events->items()[1]->color_id(), "3");
  // Verifies that an event without a colorId in the response yields an event
  // object with an empty colorId if no calendar_color_id is passed.
  EXPECT_EQ(events->items()[2]->color_id(), "");
}

// Tests that CalendarApiEventsRequest can generate the correct url and get the
// correct event list response with extra parameters and attachments.
TEST_F(CalendarApiRequestsTest,
       GetEventListRequestWithExtraParametersAndAttachments) {
  ApiErrorCode error = OTHER_ERROR;
  std::unique_ptr<EventList> events;
  base::Time start;
  base::Time end;

  EXPECT_TRUE(base::Time::FromString("13 Jun 2021 10:00 GMT", &start));
  EXPECT_TRUE(base::Time::FromString("16 Jun 2021 10:00 GMT", &end));

  {
    base::RunLoop run_loop;
    std::vector<EventType> event_types;
    event_types.push_back(EventType::kDefault);
    auto request = std::make_unique<CalendarApiEventsRequest>(
        request_sender_.get(), *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop, test_util::CreateCopyResultCallback(&error, &events)),
        start, end, event_types,
        /*experiment=*/"test",
        /*order_by=*/"startTime",
        /*attachments=*/true);

    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(
      "/calendar/v3/calendars/primary/events"
      "?timeMin=2021-06-13T10%3A00%3A00.000Z"
      "&timeMax=2021-06-16T10%3A00%3A00.000Z"
      "&singleEvents=true"
      "&maxResults=2500"
      "&orderBy=startTime"
      "&eventTypes=default"
      "&experiment=test"
      "&fields=timeZone%2Cetag%2Ckind%2Citems(id%2Ckind%"
      "2Csummary%2CcolorId%2Cstatus%"
      "2Cstart(date)%2Cend(date)%"
      "2Cstart(dateTime)%2Cend(dateTime)%"
      "2ChtmlLink%2Cattendees(responseStatus%2Cself)%2CattendeesOmitted%"
      "2CconferenceData(conferenceId%2CentryPoints(entryPointType%2Curi))%"
      "2Ccreator(self)%2Clocation%"
      "2Cattachments(title%2CfileUrl%2CiconLink%2CfileId))",
      http_request_.relative_url);

  ASSERT_TRUE(events.get());

  EXPECT_EQ(events->time_zone(), "America/Los_Angeles");
  base::Time::Exploded exploded;
  events->items()[0]->start_time().date_time().LocalExplode(&exploded);
  EXPECT_EQ(exploded.month, 11);
  EXPECT_EQ(events->items()[0]->color_id(), "3");
  EXPECT_EQ(events->items()[1]->color_id(), "3");
  // Verifies that an event without a colorId in the response yields an event
  // object with an empty colorId if no calendar_color_id is passed.
  EXPECT_EQ(events->items()[2]->color_id(), "");
}

// Tests that CalendarApiEventsRequest can generate the correct url when
// attachments are requested.
TEST_F(CalendarApiRequestsTest, GetEventListRequestWithAttachments) {
  ApiErrorCode error = OTHER_ERROR;
  std::unique_ptr<EventList> events;
  base::Time start;
  base::Time end;

  EXPECT_TRUE(base::Time::FromString("13 Jun 2021 10:00 GMT", &start));
  EXPECT_TRUE(base::Time::FromString("16 Jun 2021 10:00 GMT", &end));

  {
    base::RunLoop run_loop;
    auto request = std::make_unique<CalendarApiEventsRequest>(
        request_sender_.get(), *url_generator_,
        test_util::CreateQuitCallback(
            &run_loop, test_util::CreateCopyResultCallback(&error, &events)),
        start, end, /*include_attachments=*/true);

    request_sender_->StartRequestWithAuthRetry(std::move(request));
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ(
      "/calendar/v3/calendars/primary/events"
      "?timeMin=2021-06-13T10%3A00%3A00.000Z"
      "&timeMax=2021-06-16T10%3A00%3A00.000Z"
      "&singleEvents=true"
      "&maxAttendees=1"
      "&maxResults=2500"
      "&fields=timeZone%2Cetag%2Ckind%2Citems(id%2Ckind"
      "%2Csummary%2CcolorId%2Cstatus"
      "%2Cstart(date)%2Cend(date)"
      "%2Cstart(dateTime)%2Cend(dateTime)"
      "%2ChtmlLink%2Cattendees(responseStatus%2Cself)%2CattendeesOmitted"
      "%2CconferenceData(conferenceId%2CentryPoints(entryPointType%2Curi))"
      "%2Ccreator(self)%2Clocation"
      "%2Cattachments(title%2CfileUrl%2CiconLink%2CfileId))",
      http_request_.relative_url);
}

}  // namespace calendar
}  // namespace google_apis
