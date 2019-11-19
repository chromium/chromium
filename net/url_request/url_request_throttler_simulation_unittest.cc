// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests in this file attempt to verify the following through simulation:
// a) That a server experiencing overload will actually benefit from the
//    anti-DDoS throttling logic, i.e. that its traffic spike will subside
//    and be distributed over a longer period of time;
// b) That "well-behaved" clients of a server under DDoS attack actually
//    benefit from the anti-DDoS throttling logic; and
// c) That the approximate increase in "perceived downtime" introduced by
//    anti-DDoS throttling for various different actual downtimes is what
//    we expect it to be.

#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "base/environment.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "net/url_request/url_request_throttler_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {
namespace {

// Set this variable in your environment if you want to see verbose results
// of the simulation tests.
const char kShowSimulationVariableName[] = "SHOW_SIMULATION_RESULTS";

// Prints output only if a given environment variable is set. We use this
// to not print any output for human evaluation when the test is run without
// supervision.
void VerboseOut(const char* format, ...) {
  static bool have_checked_environment = false;
  static bool should_print = false;
  if (!have_checked_environment) {
    have_checked_environment = true;
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    if (env->HasVar(kShowSimulationVariableName))
      should_print = true;
  }

  if (should_print) {
    va_list arglist;
    va_start(arglist, format);
    vprintf(format, arglist);
    va_end(arglist);
  }
}

// A simple two-phase discrete time simulation. Actors are added in the order
// they should take action at every tick of the clock. Ticks of the clock
// are two-phase:
// - Phase 1 advances every actor's time to a new absolute time.
// - Phase 2 asks each actor to perform their action.
class DiscreteTimeSimulation {
 public:
  class Actor {
   public:
    virtual ~Actor() = default;
    virtual void AdvanceTime(const TimeTicks& absolute_time) = 0;
    virtual void PerformAction() = 0;
  };

  DiscreteTimeSimulation() = default;

  // Adds an |actor| to the simulation. The client of the simulation maintains
  // ownership of |actor| and must ensure its lifetime exceeds that of the
  // simulation. Actors should be added in the order you wish for them to
  // act at each tick of the simulation.
  void AddActor(Actor* actor) {
    actors_.push_back(actor);
  }

  // Runs the simulation for, pretending |time_between_ticks| passes from one
  // tick to the next. The start time will be the current real time. The
  // simulation will stop when the simulated duration is equal to or greater
  // than |maximum_simulated_duration|.
  void RunSimulation(const TimeDelta& maximum_simulated_duration,
                     const TimeDelta& time_between_ticks) {
    TimeTicks start_time = TimeTicks();
    TimeTicks now = start_time;
    while ((now - start_time) <= maximum_simulated_duration) {
      for (auto it = actors_.begin(); it != actors_.end(); ++it) {
        (*it)->AdvanceTime(now);
      }

      for (auto it = actors_.begin(); it != actors_.end(); ++it) {
        (*it)->PerformAction();
      }

      now += time_between_ticks;
    }
  }

 private:
  std::vector<Actor*> actors_;

  DISALLOW_COPY_AND_ASSIGN(DiscreteTimeSimulation);
};

// Represents a web server in a simulation of a server under attack by
// a lot of clients. Must be added to the simulation's list of actors
// after all |Requester| objects.
class Server : public DiscreteTimeSimulation::Actor {
 public:
  Server(int max_queries_per_tick, double request_drop_ratio)
      : max_queries_per_tick_(max_queries_per_tick),
        request_drop_ratio_(request_drop_ratio),
        num_overloaded_ticks_remaining_(0),
        num_current_tick_queries_(0),
        num_overloaded_ticks_(0),
        max_experienced_queries_per_tick_(0),
        mock_request_(context_.CreateRequest(GURL(),
                                             DEFAULT_PRIORITY,
                                             nullptr,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  void SetDowntime(const TimeTicks& start_time, const TimeDelta& duration) {
    start_downtime_ = start_time;
    end_downtime_ = start_time + duration;
  }

  void AdvanceTime(const TimeTicks& absolute_time) override {
    now_ = absolute_time;
  }

  void PerformAction() override {
    // We are inserted at the end of the actor's list, so all Requester
    // instances have already done their bit.
    if (num_current_tick_queries_ > max_experienced_queries_per_tick_)
      max_experienced_queries_per_tick_ = num_current_tick_queries_;

    if (num_current_tick_queries_ > max_queries_per_tick_) {
      // We pretend the server fails for the next several ticks after it
      // gets overloaded.
      num_overloaded_ticks_remaining_ = 5;
      ++num_overloaded_ticks_;
    } else if (num_overloaded_ticks_remaining_ > 0) {
      --num_overloaded_ticks_remaining_;
    }

    requests_per_tick_.push_back(num_current_tick_queries_);
    num_current_tick_queries_ = 0;
  }

  // This is called by Requester. It returns the response code from
  // the server.
  int HandleRequest() {
    ++num_current_tick_queries_;
    if (!start_downtime_.is_null() &&
        start_downtime_ < now_ && now_ < end_downtime_) {
      // For the simulation measuring the increase in perceived
      // downtime, it might be interesting to count separately the
      // queries seen by the server (assuming a front-end reverse proxy
      // is what actually serves up the 503s in this case) so that we could
      // visualize the traffic spike seen by the server when it comes up,
      // which would in many situations be ameliorated by the anti-DDoS
      // throttling.
      return 503;
    }

    if ((num_overloaded_ticks_remaining_ > 0 ||
         num_current_tick_queries_ > max_queries_per_tick_) &&
        base::RandDouble() < request_drop_ratio_) {
      return 503;
    }

    return 200;
  }

  int num_overloaded_ticks() const {
    return num_overloaded_ticks_;
  }

  int max_experienced_queries_per_tick() const {
    return max_experienced_queries_per_tick_;
  }

  const URLRequest& mock_request() const {
    return *mock_request_.get();
  }

  std::string VisualizeASCII(int terminal_width) {
    // Account for | characters we place at left of graph.
    terminal_width -= 1;

    VerboseOut("Overloaded for %d of %d ticks.\n",
               num_overloaded_ticks_, requests_per_tick_.size());
    VerboseOut("Got maximum of %d requests in a tick.\n\n",
               max_experienced_queries_per_tick_);

    VerboseOut("Traffic graph:\n\n");

    // Printing the graph like this is a bit overkill, but was very useful
    // while developing the various simulations to see if they were testing
    // the corner cases we want to simulate.

    // Find the smallest number of whole ticks we need to group into a
    // column that will let all ticks fit into the column width we have.
    int num_ticks = requests_per_tick_.size();
    double ticks_per_column_exact =
        static_cast<double>(num_ticks) / static_cast<double>(terminal_width);
    int ticks_per_column = std::ceil(ticks_per_column_exact);
    DCHECK_GE(ticks_per_column * terminal_width, num_ticks);

    // Sum up the column values.
    int num_columns = num_ticks / ticks_per_column;
    if (num_ticks % ticks_per_column)
      ++num_columns;
    DCHECK_LE(num_columns, terminal_width);
    std::unique_ptr<int[]> columns(new int[num_columns]);
    for (int tx = 0; tx < num_ticks; ++tx) {
      int cx = tx / ticks_per_column;
      if (tx % ticks_per_column == 0)
        columns[cx] = 0;
      columns[cx] += requests_per_tick_[tx];
    }

    // Find the lowest integer divisor that will let the column values
    // be represented in a graph of maximum height 50.
    int max_value = 0;
    for (int cx = 0; cx < num_columns; ++cx)
      max_value = std::max(max_value, columns[cx]);
    const int kNumRows = 50;
    double row_divisor_exact = max_value / static_cast<double>(kNumRows);
    int row_divisor = std::ceil(row_divisor_exact);
    DCHECK_GE(row_divisor * kNumRows, max_value);

    // To show the overload line, we calculate the appropriate value.
    int overload_value = max_queries_per_tick_ * ticks_per_column;

    // When num_ticks is not a whole multiple of ticks_per_column, the last
    // column includes fewer ticks than the others. In this case, don't
    // print it so that we don't show an inconsistent value.
    int num_printed_columns = num_columns;
    if (num_ticks % ticks_per_column)
      --num_printed_columns;

    // This is a top-to-bottom traversal of rows, left-to-right per row.
    std::string output;
    for (int rx = 0; rx < kNumRows; ++rx) {
      int range_min = (kNumRows - rx) * row_divisor;
      int range_max = range_min + row_divisor;
      if (range_min == 0)
        range_min = -1;  // Make 0 values fit in the bottom range.
      output.append("|");
      for (int cx = 0; cx < num_printed_columns; ++cx) {
        char block = ' ';
        // Show the overload line.
        if (range_min < overload_value && overload_value <= range_max)
          block = '-';

        // Preferentially, show the graph line.
        if (range_min < columns[cx] && columns[cx] <= range_max)
          block = '#';

        output.append(1, block);
      }
      output.append("\n");
    }
    output.append("|");
    output.append(num_printed_columns, '=');

    return output;
  }

  const URLRequestContext& context() const { return context_; }

 private:
  TimeTicks now_;
  TimeTicks start_downtime_;  // Can be 0 to say "no downtime".
  TimeTicks end_downtime_;
  const int max_queries_per_tick_;
  const double request_drop_ratio_;  // Ratio of requests to 503 when failing.
  int num_overloaded_ticks_remaining_;
  int num_current_tick_queries_;
  int num_overloaded_ticks_;
  int max_experienced_queries_per_tick_;
  std::vector<int> requests_per_tick_;

  TestURLRequestContext context_;
  std::unique_ptr<URLRequest> mock_request_;

  DISALLOW_COPY_AND_ASSIGN(Server);
};

// Mock throttler entry used by Requester class.
class MockURLRequestThrottlerEntry : public URLRequestThrottlerEntry {
 public:
  explicit MockURLRequestThrottlerEntry(URLRequestThrottlerManager* manager)
      : URLRequestThrottlerEntry(manager, std::string()),
        backoff_entry_(&backoff_policy_, &fake_clock_) {}

  const BackoffEntry* GetBackoffEntry() const override {
    return &backoff_entry_;
  }

  BackoffEntry* GetBackoffEntry() override { return &backoff_entry_; }

  TimeTicks ImplGetTimeNow() const override { return fake_clock_.NowTicks(); }

  void SetFakeNow(const TimeTicks& fake_time) {
    fake_clock_.set_now(fake_time);
  }

 protected:
  ~MockURLRequestThrottlerEntry() override = default;

 private:
  mutable TestTickClock fake_clock_;
  BackoffEntry backoff_entry_;
};

// Registry of results for a class of |Requester| objects (e.g. attackers vs.
// regular clients).
class RequesterResults {
 public:
  RequesterResults()
      : num_attempts_(0), num_successful_(0), num_failed_(0), num_blocked_(0) {
  }

  void AddSuccess() {
    ++num_attempts_;
    ++num_successful_;
  }

  void AddFailure() {
    ++num_attempts_;
    ++num_failed_;
  }

  void AddBlocked() {
    ++num_attempts_;
    ++num_blocked_;
  }

  int num_attempts() const { return num_attempts_; }
  int num_successful() const { return num_successful_; }
  int num_failed() const { return num_failed_; }
  int num_blocked() const { return num_blocked_; }

  double GetBlockedRatio() {
    DCHECK(num_attempts_);
    return static_cast<double>(num_blocked_) /
        static_cast<double>(num_attempts_);
  }

  double GetSuccessRatio() {
    DCHECK(num_attempts_);
    return static_cast<double>(num_successful_) /
        static_cast<double>(num_attempts_);
  }

  void PrintResults(const char* class_description) {
    if (num_attempts_ == 0) {
      VerboseOut("No data for %s\n", class_description);
      return;
    }

    VerboseOut("Requester results for %s\n", class_description);
    VerboseOut("  %d attempts\n", num_attempts_);
    VerboseOut("  %d successes\n", num_successful_);
    VerboseOut("  %d 5xx responses\n", num_failed_);
    VerboseOut("  %d requests blocked\n", num_blocked_);
    VerboseOut("  %.2f success ratio\n", GetSuccessRatio());
    VerboseOut("  %.2f blocked ratio\n", GetBlockedRatio());
    VerboseOut("\n");
  }

 private:
  int num_attempts_;
  int num_successful_;
  int num_failed_;
  int num_blocked_;
};

// Represents an Requester in a simulated DDoS situation, that periodically
// requests a specific resource.
class Requester : public DiscreteTimeSimulation::Actor {
 public:
  Requester(MockURLRequestThrottlerEntry* throttler_entry,
            const TimeDelta& time_between_requests,
            Server* server,
            RequesterResults* results)
      : throttler_entry_(throttler_entry),
        time_between_requests_(time_between_requests),
        last_attempt_was_failure_(false),
        server_(server),
        results_(results) {
    DCHECK(server_);
  }

  void AdvanceTime(const TimeTicks& absolute_time) override {
    if (time_of_last_success_.is_null())
      time_of_last_success_ = absolute_time;

    throttler_entry_->SetFakeNow(absolute_time);
  }

  void PerformAction() override {
    TimeDelta effective_delay = time_between_requests_;
    TimeDelta current_jitter = TimeDelta::FromMilliseconds(
        request_jitter_.InMilliseconds() * base::RandDouble());
    if (base::RandInt(0, 1)) {
      effective_delay -= current_jitter;
    } else {
      effective_delay += current_jitter;
    }

    if (throttler_entry_->ImplGetTimeNow() - time_of_last_attempt_ >
        effective_delay) {
      if (!throttler_entry_->ShouldRejectRequest(server_->mock_request())) {
        int status_code = server_->HandleRequest();
        throttler_entry_->UpdateWithResponse(status_code);

        if (status_code == 200) {
          if (results_)
            results_->AddSuccess();

          if (last_attempt_was_failure_) {
            last_downtime_duration_ =
                throttler_entry_->ImplGetTimeNow() - time_of_last_success_;
          }

          time_of_last_success_ = throttler_entry_->ImplGetTimeNow();
          last_attempt_was_failure_ = false;
        } else {
          if (results_)
            results_->AddFailure();
          last_attempt_was_failure_ = true;
        }
      } else {
        if (results_)
          results_->AddBlocked();
        last_attempt_was_failure_ = true;
      }

      time_of_last_attempt_ = throttler_entry_->ImplGetTimeNow();
    }
  }

  // Adds a delay until the first request, equal to a uniformly distributed
  // value between now and now + max_delay.
  void SetStartupJitter(const TimeDelta& max_delay) {
    int delay_ms = base::RandInt(0, max_delay.InMilliseconds());
    time_of_last_attempt_ = TimeTicks() +
        TimeDelta::FromMilliseconds(delay_ms) - time_between_requests_;
  }

  void SetRequestJitter(const TimeDelta& request_jitter) {
    request_jitter_ = request_jitter;
  }

  TimeDelta last_downtime_duration() const { return last_downtime_duration_; }

 private:
  scoped_refptr<MockURLRequestThrottlerEntry> throttler_entry_;
  const TimeDelta time_between_requests_;
  TimeDelta request_jitter_;
  TimeTicks time_of_last_attempt_;
  TimeTicks time_of_last_success_;
  bool last_attempt_was_failure_;
  TimeDelta last_downtime_duration_;
  Server* const server_;
  RequesterResults* const results_;  // May be NULL.

  DISALLOW_COPY_AND_ASSIGN(Requester);
};

void SimulateAttack(Server* server,
                    RequesterResults* attacker_results,
                    RequesterResults* client_results,
                    bool enable_throttling) {
  const size_t kNumAttackers = 50;
  const size_t kNumClients = 50;
  DiscreteTimeSimulation simulation;
  URLRequestThrottlerManager manager;
  std::vector<std::unique_ptr<Requester>> requesters;
  for (size_t i = 0; i < kNumAttackers; ++i) {
    // Use a tiny time_between_requests so the attackers will ping the
    // server at every tick of the simulation.
    scoped_refptr<MockURLRequestThrottlerEntry> throttler_entry(
        new MockURLRequestThrottlerEntry(&manager));
    if (!enable_throttling)
      throttler_entry->DisableBackoffThrottling();

    std::unique_ptr<Requester> attacker(
        new Requester(throttler_entry.get(), TimeDelta::FromMilliseconds(1),
                      server, attacker_results));
    attacker->SetStartupJitter(TimeDelta::FromSeconds(120));
    simulation.AddActor(attacker.get());
    requesters.push_back(std::move(attacker));
  }
  for (size_t i = 0; i < kNumClients; ++i) {
    // Normal clients only make requests every 2 minutes, plus/minus 1 minute.
    scoped_refptr<MockURLRequestThrottlerEntry> throttler_entry(
        new MockURLRequestThrottlerEntry(&manager));
    if (!enable_throttling)
      throttler_entry->DisableBackoffThrottling();

    std::unique_ptr<Requester> client(new Requester(throttler_entry.get(),
                                                    TimeDelta::FromMinutes(2),
                                                    server, client_results));
    client->SetStartupJitter(TimeDelta::FromSeconds(120));
    client->SetRequestJitter(TimeDelta::FromMinutes(1));
    simulation.AddActor(client.get());
    requesters.push_back(std::move(client));
  }
  simulation.AddActor(server);

  simulation.RunSimulation(TimeDelta::FromMinutes(6),
                           TimeDelta::FromSeconds(1));
}

TEST(URLRequestThrottlerSimulation, HelpsInAttack) {
  base::test::TaskEnvironment task_environment;

  Server unprotected_server(30, 1.0);
  RequesterResults unprotected_attacker_results;
  RequesterResults unprotected_client_results;
  Server protected_server(30, 1.0);
  RequesterResults protected_attacker_results;
  RequesterResults protected_client_results;
  SimulateAttack(&unprotected_server,
                 &unprotected_attacker_results,
                 &unprotected_client_results,
                 false);
  SimulateAttack(&protected_server,
                 &protected_attacker_results,
                 &protected_client_results,
                 true);

  // These assert that the DDoS protection actually benefits the
  // server. Manual inspection of the traffic graphs will show this
  // even more clearly.
  EXPECT_GT(unprotected_server.num_overloaded_ticks(),
            protected_server.num_overloaded_ticks());
  EXPECT_GT(unprotected_server.max_experienced_queries_per_tick(),
            protected_server.max_experienced_queries_per_tick());

  // These assert that the DDoS protection actually benefits non-malicious
  // (and non-degenerate/accidentally DDoSing) users.
  EXPECT_LT(protected_client_results.GetBlockedRatio(),
            protected_attacker_results.GetBlockedRatio());
  EXPECT_GT(protected_client_results.GetSuccessRatio(),
            unprotected_client_results.GetSuccessRatio());

  // The rest is just for optional manual evaluation of the results;
  // in particular the traffic pattern is interesting.

  VerboseOut("\nUnprotected server's results:\n\n");
  VerboseOut(unprotected_server.VisualizeASCII(132).c_str());
  VerboseOut("\n\n");
  VerboseOut("Protected server's results:\n\n");
  VerboseOut(protected_server.VisualizeASCII(132).c_str());
  VerboseOut("\n\n");

  unprotected_attacker_results.PrintResults(
      "attackers attacking unprotected server.");
  unprotected_client_results.PrintResults(
      "normal clients making requests to unprotected server.");
  protected_attacker_results.PrintResults(
      "attackers attacking protected server.");
  protected_client_results.PrintResults(
      "normal clients making requests to protected server.");
}

// Returns the downtime perceived by the client, as a ratio of the
// actual downtime.
double SimulateDowntime(const TimeDelta& duration,
                        const TimeDelta& average_client_interval,
                        bool enable_throttling) {
  TimeDelta time_between_ticks = duration / 200;
  TimeTicks start_downtime = TimeTicks() + (duration / 2);

  // A server that never rejects requests, but will go down for maintenance.
  Server server(std::numeric_limits<int>::max(), 1.0);
  server.SetDowntime(start_downtime, duration);

  URLRequestThrottlerManager manager;
  scoped_refptr<MockURLRequestThrottlerEntry> throttler_entry(
      new MockURLRequestThrottlerEntry(&manager));
  if (!enable_throttling)
    throttler_entry->DisableBackoffThrottling();

  Requester requester(throttler_entry.get(), average_client_interval, &server,
                      nullptr);
  requester.SetStartupJitter(duration / 3);
  requester.SetRequestJitter(average_client_interval);

  DiscreteTimeSimulation simulation;
  simulation.AddActor(&requester);
  simulation.AddActor(&server);

  simulation.RunSimulation(duration * 2, time_between_ticks);

  return static_cast<double>(
      requester.last_downtime_duration().InMilliseconds()) /
      static_cast<double>(duration.InMilliseconds());
}

TEST(URLRequestThrottlerSimulation, PerceivedDowntimeRatio) {
  base::test::TaskEnvironment task_environment;

  struct Stats {
    // Expected interval that we expect the ratio of downtime when anti-DDoS
    // is enabled and downtime when anti-DDoS is not enabled to fall within.
    //
    // The expected interval depends on two things:  The exponential back-off
    // policy encoded in URLRequestThrottlerEntry, and the test or set of
    // tests that the Stats object is tracking (e.g. a test where the client
    // retries very rapidly on a very long downtime will tend to increase the
    // number).
    //
    // To determine an appropriate new interval when parameters have changed,
    // run the test a few times (you may have to Ctrl-C out of it after a few
    // seconds) and choose an interval that the test converges quickly and
    // reliably to.  Then set the new interval, and run the test e.g. 20 times
    // in succession to make sure it never takes an obscenely long time to
    // converge to this interval.
    double expected_min_increase;
    double expected_max_increase;

    size_t num_runs;
    double total_ratio_unprotected;
    double total_ratio_protected;

    bool DidConverge(double* increase_ratio_out) {
      double unprotected_ratio = total_ratio_unprotected / num_runs;
      double protected_ratio = total_ratio_protected / num_runs;
      double increase_ratio = protected_ratio / unprotected_ratio;
      if (increase_ratio_out)
        *increase_ratio_out = increase_ratio;
      return expected_min_increase <= increase_ratio &&
          increase_ratio <= expected_max_increase;
    }

    void ReportTrialResult(double increase_ratio) {
      VerboseOut(
          "  Perceived downtime with throttling is %.4f times without.\n",
          increase_ratio);
      VerboseOut("  Test result after %d trials.\n", num_runs);
    }
  };

  Stats global_stats = { 1.08, 1.15 };

  struct Trial {
    TimeDelta duration;
    TimeDelta average_client_interval;
    Stats stats;

    void PrintTrialDescription() {
      double duration_minutes =
          static_cast<double>(duration.InSeconds()) / 60.0;
      double interval_minutes =
          static_cast<double>(average_client_interval.InSeconds()) / 60.0;
      VerboseOut("Trial with %.2f min downtime, avg. interval %.2f min.\n",
                 duration_minutes, interval_minutes);
    }
  };

  // We don't set or check expected ratio intervals on individual
  // experiments as this might make the test too fragile, but we
  // print them out at the end for manual evaluation (we want to be
  // able to make claims about the expected ratios depending on the
  // type of behavior of the client and the downtime, e.g. the difference
  // in behavior between a client making requests every few minutes vs.
  // one that makes a request every 15 seconds).
  Trial trials[] = {
    { TimeDelta::FromSeconds(10), TimeDelta::FromSeconds(3) },
    { TimeDelta::FromSeconds(30), TimeDelta::FromSeconds(7) },
    { TimeDelta::FromMinutes(5), TimeDelta::FromSeconds(30) },
    { TimeDelta::FromMinutes(10), TimeDelta::FromSeconds(20) },
    { TimeDelta::FromMinutes(20), TimeDelta::FromSeconds(15) },
    { TimeDelta::FromMinutes(20), TimeDelta::FromSeconds(50) },
    { TimeDelta::FromMinutes(30), TimeDelta::FromMinutes(2) },
    { TimeDelta::FromMinutes(30), TimeDelta::FromMinutes(5) },
    { TimeDelta::FromMinutes(40), TimeDelta::FromMinutes(7) },
    { TimeDelta::FromMinutes(40), TimeDelta::FromMinutes(2) },
    { TimeDelta::FromMinutes(40), TimeDelta::FromSeconds(15) },
    { TimeDelta::FromMinutes(60), TimeDelta::FromMinutes(7) },
    { TimeDelta::FromMinutes(60), TimeDelta::FromMinutes(2) },
    { TimeDelta::FromMinutes(60), TimeDelta::FromSeconds(15) },
    { TimeDelta::FromMinutes(80), TimeDelta::FromMinutes(20) },
    { TimeDelta::FromMinutes(80), TimeDelta::FromMinutes(3) },
    { TimeDelta::FromMinutes(80), TimeDelta::FromSeconds(15) },

    // Most brutal?
    { TimeDelta::FromMinutes(45), TimeDelta::FromMilliseconds(500) },
  };

  // If things don't converge by the time we've done 100K trials, then
  // clearly one or more of the expected intervals are wrong.
  while (global_stats.num_runs < 100000) {
    for (size_t i = 0; i < base::size(trials); ++i) {
      ++global_stats.num_runs;
      ++trials[i].stats.num_runs;
      double ratio_unprotected = SimulateDowntime(
          trials[i].duration, trials[i].average_client_interval, false);
      double ratio_protected = SimulateDowntime(
          trials[i].duration, trials[i].average_client_interval, true);
      global_stats.total_ratio_unprotected += ratio_unprotected;
      global_stats.total_ratio_protected += ratio_protected;
      trials[i].stats.total_ratio_unprotected += ratio_unprotected;
      trials[i].stats.total_ratio_protected += ratio_protected;
    }

    double increase_ratio;
    if (global_stats.DidConverge(&increase_ratio))
      break;

    if (global_stats.num_runs > 200) {
      VerboseOut("Test has not yet converged on expected interval.\n");
      global_stats.ReportTrialResult(increase_ratio);
    }
  }

  double average_increase_ratio;
  EXPECT_TRUE(global_stats.DidConverge(&average_increase_ratio));

  // Print individual trial results for optional manual evaluation.
  double max_increase_ratio = 0.0;
  for (size_t i = 0; i < base::size(trials); ++i) {
    double increase_ratio;
    trials[i].stats.DidConverge(&increase_ratio);
    max_increase_ratio = std::max(max_increase_ratio, increase_ratio);
    trials[i].PrintTrialDescription();
    trials[i].stats.ReportTrialResult(increase_ratio);
  }

  VerboseOut("Average increase ratio was %.4f\n", average_increase_ratio);
  VerboseOut("Maximum increase ratio was %.4f\n", max_increase_ratio);
}

}  // namespace
}  // namespace net
