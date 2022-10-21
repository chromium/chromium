// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/highest_pmf_reporter.h"

#include <limits>
#include "base/metrics/histogram_functions.h"
#include "base/task/task_runner.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

const char* HighestPmfReporter::highest_pmf_metric_names[] = {
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.0to2min",
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.2to4min",
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.4to8min",
    "Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.8to16min"};

const char* HighestPmfReporter::peak_resident_bytes_metric_names[] = {
    "Memory.Experimental.Renderer.PeakResidentSet."
    "AtHighestPrivateMemoryFootprint.0to2min",
    "Memory.Experimental.Renderer.PeakResidentSet."
    "AtHighestPrivateMemoryFootprint.2to4min",
    "Memory.Experimental.Renderer.PeakResidentSet."
    "AtHighestPrivateMemoryFootprint.4to8min",
    "Memory.Experimental.Renderer.PeakResidentSet."
    "AtHighestPrivateMemoryFootprint.8to16min"};

const char* HighestPmfReporter::webpage_counts_metric_names[] = {
    "Memory.Experimental.Renderer.WebpageCount.AtHighestPrivateMemoryFootprint."
    "0to2min",
    "Memory.Experimental.Renderer.WebpageCount.AtHighestPrivateMemoryFootprint."
    "2to4min",
    "Memory.Experimental.Renderer.WebpageCount.AtHighestPrivateMemoryFootprint."
    "4to8min",
    "Memory.Experimental.Renderer.WebpageCount.AtHighestPrivateMemoryFootprint."
    "8to16min"};

constexpr base::TimeDelta HighestPmfReporter::time_to_report[] = {
    base::Minutes(2), base::Minutes(4), base::Minutes(8), base::Minutes(16)};

void HighestPmfReporter::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DEFINE_STATIC_LOCAL(HighestPmfReporter, reporter, (std::move(task_runner)));
  (void)reporter;
}

HighestPmfReporter::HighestPmfReporter(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : HighestPmfReporter(std::move(task_runner),
                         base::DefaultTickClock::GetInstance()) {}

HighestPmfReporter::HighestPmfReporter(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* clock)
    : task_runner_(std::move(task_runner)), clock_(clock) {
  MemoryUsageMonitor::Instance().AddObserver(this);
}

bool HighestPmfReporter::FirstNavigationStarted() {
  if (first_navigation_detected_)
    return false;

  for (Page* page : Page::OrdinaryPages()) {
    Frame* frame = page->MainFrame();
    if (!frame)
      continue;

    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;

    DocumentLoader* loader = local_frame->Loader().GetDocumentLoader();
    if (!loader)
      continue;

    if (!loader->GetTiming().NavigationStart().is_null()) {
      first_navigation_detected_ = true;
      return true;
    }
  }
  return false;
}

void HighestPmfReporter::OnMemoryPing(MemoryUsage usage) {
  DCHECK(IsMainThread());
  if (FirstNavigationStarted()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&HighestPmfReporter::OnReportMetrics,
                      WTF::Unretained(this)),
        time_to_report[0]);
  }

  if (current_highest_pmf_ > usage.private_footprint_bytes)
    return;

  current_highest_pmf_ = usage.private_footprint_bytes;
  peak_resident_bytes_at_current_highest_pmf_ = usage.peak_resident_bytes;
  webpage_counts_at_current_highest_pmf_ = Page::OrdinaryPages().size();

  // TODO(tasak): Report the highest memory footprint throughout renderer's
  // lifetime.
}

void HighestPmfReporter::OnReportMetrics() {
  DCHECK(IsMainThread());
  ReportMetrics();

  // The following code is not accurate, because OnReportMetrics will be late
  // when renderer is slow (e.g. caused by near-OOM or heavy tasks is running
  // or ...). However such signal getting late by minutes is unlikely, so it's
  // ok to say "this is good enough".
  current_highest_pmf_ = 0.0;
  peak_resident_bytes_at_current_highest_pmf_ = 0.0;
  webpage_counts_at_current_highest_pmf_ = 0;
  report_count_++;
  if (report_count_ >= std::size(time_to_report)) {
    // Stop observing the MemoryUsageMonitor once there's no more histogram to
    // report.
    MemoryUsageMonitor::Instance().RemoveObserver(this);
    return;
  }

  base::TimeDelta delay =
      time_to_report[report_count_] - time_to_report[report_count_ - 1];
  task_runner_->PostDelayedTask(
      FROM_HERE,
      WTF::BindOnce(&HighestPmfReporter::OnReportMetrics,
                    WTF::Unretained(this)),
      delay);
}

void HighestPmfReporter::ReportMetrics() {
  base::UmaHistogramMemoryMB(highest_pmf_metric_names[report_count_],
                             base::saturated_cast<base::Histogram::Sample>(
                                 current_highest_pmf_ / 1024 / 1024));

  base::UmaHistogramMemoryMB(
      peak_resident_bytes_metric_names[report_count_],
      base::saturated_cast<base::Histogram::Sample>(
          peak_resident_bytes_at_current_highest_pmf_ / 1024 / 1024));

  base::UmaHistogramCounts100(webpage_counts_metric_names[report_count_],
                              webpage_counts_at_current_highest_pmf_);
}

}  // namespace blink
