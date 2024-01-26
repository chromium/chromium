// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_server_iterator.h"

#include <optional>

#include "base/time/time.h"
#include "net/dns/dns_session.h"
#include "net/dns/resolve_context.h"

namespace net {
DnsServerIterator::DnsServerIterator(size_t nameservers_size,
                                     size_t starting_index,
                                     int max_times_returned,
                                     int max_failures,
                                     const ResolveContext* resolve_context,
                                     const DnsSession* session)
    : times_returned_(nameservers_size, 0),
      max_times_returned_(max_times_returned),
      max_failures_(max_failures),
      resolve_context_(resolve_context),
      next_index_(starting_index),
      session_(session) {}

DnsServerIterator::~DnsServerIterator() = default;

size_t DohDnsServerIterator::GetNextAttemptIndex() {
  DCHECK(resolve_context_->IsCurrentSession(session_));
  DCHECK(AttemptAvailable());

  // Because AttemptAvailable() should always be true before running this
  // function we can assume that an attemptable DoH server exists.

  // Check if the next index is available and hasn't hit its failure limit. If
  // not, try the next one and so on until we've tried them all.
  std::optional<size_t> least_recently_failed_index;
  base::TimeTicks least_recently_failed_time;

  size_t previous_index = next_index_;
  size_t curr_index;

  do {
    curr_index = next_index_;
    next_index_ = (next_index_ + 1) % times_returned_.size();

    // If the DoH mode is "secure" then don't check GetDohServerAvailability()
    // because we try every server regardless of availability.
    bool secure_or_available_server =
        secure_dns_mode_ == SecureDnsMode::kSecure ||
        resolve_context_->GetDohServerAvailability(curr_index, session_);

    // If we've tried this server |max_times_returned_| already, then we're done
    // with it. Similarly skip this server if it isn't available and we're not
    // in secure mode.
    if (times_returned_[curr_index] >= max_times_returned_ ||
        !secure_or_available_server)
      continue;

    if (resolve_context_->doh_server_stats_[curr_index].last_failure_count <
        max_failures_) {
      times_returned_[curr_index]++;
      return curr_index;
    }

    // Update the least recently failed server if needed.
    base::TimeTicks curr_index_failure_time =
        resolve_context_->doh_server_stats_[curr_index].last_failure;
    if (!least_recently_failed_index ||
        curr_index_failure_time < least_recently_failed_time) {
      least_recently_failed_time = curr_index_failure_time;
      least_recently_failed_index = curr_index;
    }
  } while (next_index_ != previous_index);

  // At this point the only available servers we haven't attempted
  // |max_times_returned_| times are at their failure limit. Return the server
  // with the least recent failure.

  DCHECK(least_recently_failed_index.has_value());
  times_returned_[least_recently_failed_index.value()]++;
  return least_recently_failed_index.value();
}

bool DohDnsServerIterator::AttemptAvailable() {
  if (!resolve_context_->IsCurrentSession(session_))
    return false;

  for (size_t i = 0; i < times_returned_.size(); i++) {
    // If the DoH mode is "secure" then don't check GetDohServerAvailability()
    // because we try every server regardless of availability.
    bool secure_or_available_server =
        secure_dns_mode_ == SecureDnsMode::kSecure ||
        resolve_context_->GetDohServerAvailability(i, session_);

    if (times_returned_[i] < max_times_returned_ && secure_or_available_server)
      return true;
  }
  return false;
}

size_t ClassicDnsServerIterator::GetNextAttemptIndex() {
  DCHECK(resolve_context_->IsCurrentSession(session_));
  DCHECK(AttemptAvailable());

  // Because AttemptAvailable() should always be true before running this
  // function we can assume that an attemptable DNS server exists.

  // Check if the next index is available and hasn't hit its failure limit. If
  // not, try the next one and so on until we've tried them all.
  std::optional<size_t> least_recently_failed_index;
  base::TimeTicks least_recently_failed_time;

  size_t previous_index = next_index_;
  size_t curr_index;

  do {
    curr_index = next_index_;
    next_index_ = (next_index_ + 1) % times_returned_.size();

    // If we've tried this server |max_times_returned_| already, then we're done
    // with it.
    if (times_returned_[curr_index] >= max_times_returned_)
      continue;

    if (resolve_context_->classic_server_stats_[curr_index].last_failure_count <
        max_failures_) {
      times_returned_[curr_index]++;
      return curr_index;
    }

    // Update the least recently failed server if needed.
    base::TimeTicks curr_index_failure_time =
        resolve_context_->classic_server_stats_[curr_index].last_failure;
    if (!least_recently_failed_index ||
        curr_index_failure_time < least_recently_failed_time) {
      least_recently_failed_time = curr_index_failure_time;
      least_recently_failed_index = curr_index;
    }
  } while (next_index_ != previous_index);

  // At this point the only servers we haven't attempted |max_times_returned_|
  // times are at their failure limit. Return the server with the least recent
  // failure.

  DCHECK(least_recently_failed_index.has_value());
  times_returned_[least_recently_failed_index.value()]++;
  return least_recently_failed_index.value();
}

bool ClassicDnsServerIterator::AttemptAvailable() {
  if (!resolve_context_->IsCurrentSession(session_))
    return false;

  for (int i : times_returned_) {
    if (i < max_times_returned_)
      return true;
  }
  return false;
}

}  // namespace net
