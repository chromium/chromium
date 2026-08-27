// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

#if canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)
  import MetricKit
  import StateReporting

  @available(iOS 27.0, *)
  @objc
  @MainActor
  public final class MetricKitReportSubscriber: NSObject {
    private static let kDomainPrefix = "com.google.chrome.ios."

    private var task: Task<Void, Never>?

    @objc
    public static let sharedInstance = MetricKitReportSubscriber()

    private override init() {
      super.init()
    }

    @objc
    public func setEnabled(_ enabled: Bool) {
      if enabled {
        guard task == nil else { return }
        // Get all production domains
        let domains = Set(
          [
            ActivityReportDomain.tab,
            ActivityReportDomain.ntp,
            ActivityReportDomain.signin,
            ActivityReportDomain.assistant,
            ActivityReportDomain.tabgrid,
            ActivityReportDomain.settings,
          ].map { domain in
            StateReportingDomain(rawValue: Self.kDomainPrefix + domain.stringValue)
          })

        let manager = MetricManager(enabledStateReportingDomains: domains)
        task = Task { [weak self, manager] in
          for await report in manager.metricReports {
            guard !Task.isCancelled else { break }
            self?.processReport(report)
          }
        }
      } else {
        task?.cancel()
        task = nil
      }
    }

    // Decodes and processes a report from raw JSON data. This bridging method is
    // needed because `MetricReport` is a Swift-only struct and cannot be represented
    // in Objective-C++, preventing the C++ unit test from decoding it directly.
    @objc
    public func processReportForTesting(_ reportData: Data) {
      do {
        let report = try JSONDecoder().decode(MetricReport.self, from: reportData)
        processReport(report)
      } catch {
        NSLog("Error decoding report for testing: \(error)")
      }
    }

    private func processReport(_ report: MetricReport) {
      var prefixes: [String] = ["IOS.MetricKit.IncludingMismatch."]
      if let environment = report.environment,
        !environment.includesMultipleApplicationVersions
          && HistogramBridge.isCurrentVersionNumber(environment.applicationBuildVersion)
      {
        // If there is no version mismatch and the report's build version matches
        // the current app version, also report to the standard histograms.
        prefixes.append("IOS.MetricKit.")
      }

      // Group state entries by domain.
      let entriesByDomain = report.stateEntries.byStateReportingDomain

      for (domain, entries) in entriesByDomain {
        // Find the domain string representation.
        // E.g., "com.google.chrome.ios.tab" -> "Tab"
        guard let domainSuffix = getDomainSuffix(for: domain) else { continue }

        for entry in entries {
          // We only report for "Active" states.
          guard entry.state.label == "Active" else { continue }

          for result in entry.values {
            for prefix in prefixes {
              reportMetricResult(result, prefix: prefix, domainSuffix: domainSuffix)
            }
          }
        }
      }
    }

    private func getDomainSuffix(for domain: StateReportingDomain) -> String? {
      let domainStr = domain.rawValue
      guard domainStr.hasPrefix(Self.kDomainPrefix) else { return nil }
      let subDomain = String(domainStr.dropFirst(Self.kDomainPrefix.count))
      switch subDomain {
      case "tab": return "Tab"
      case "ntp": return "NTP"
      case "signin": return "Signin"
      case "assistant": return "Assistant"
      case "tabgrid": return "TabGrid"
      case "settings": return "Settings"
      default: return nil
      }
    }

    private func reportMetricResult(_ result: MetricResult, prefix: String, domainSuffix: String) {
      switch result {
      case .totalForegroundTime(let metric):
        let seconds = metric.value.converted(to: .seconds).value
        HistogramBridge.reportLongDuration(
          prefix + "ForegroundTimePerDay." + domainSuffix, withSeconds: seconds)

      case .totalBackgroundTime(let metric):
        let seconds = metric.value.converted(to: .seconds).value
        HistogramBridge.reportLongDuration(
          prefix + "BackgroundTimePerDay." + domainSuffix, withSeconds: seconds)

      case .cpuTime(let metric):
        let seconds = metric.value.converted(to: .seconds).value
        HistogramBridge.reportLongDuration(
          prefix + "CPUTimePerDay." + domainSuffix, withSeconds: seconds)

      case .suspendedMemory(let metric):
        let megabytes = metric.value.average.converted(to: .megabytes).value
        HistogramBridge.reportMemoryLargeMB(
          prefix + "AverageSuspendedMemory." + domainSuffix, withMegabytes: megabytes)

      case .peakMemory(let metric):
        let megabytes = metric.value.converted(to: .megabytes).value
        HistogramBridge.reportMemoryLargeMB(
          prefix + "PeakMemoryUsage." + domainSuffix, withMegabytes: megabytes)

      case .hangTime(let metric):
        let histogramName = prefix + "ApplicationHangTime." + domainSuffix
        for bucket in metric.histogram.buckets {
          let start = bucket.lowerBound.converted(to: .milliseconds).value
          let end = bucket.upperBound.converted(to: .milliseconds).value
          let sample = (start + end) / 2.0
          HistogramBridge.reportHangTimeBucket(
            histogramName, withSample: sample, count: bucket.count)
        }

      case .foregroundTermination(let metric):
        let histogramName = prefix + "ForegroundExitData." + domainSuffix
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.normal.rawValue,
          count: metric.normalTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.abnormal.rawValue,
          count: metric.abnormalTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.watchdog.rawValue,
          count: metric.watchdogTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.memoryLimit.rawValue,
          count: metric.memoryLimitTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.badAccess.rawValue,
          count: metric.badAccessTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.illegalInstruction.rawValue,
          count: metric.illegalInstructionTerminationCount)

      case .backgroundTermination(let metric):
        let histogramName = prefix + "BackgroundExitData." + domainSuffix
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.normal.rawValue,
          count: metric.normalTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.abnormal.rawValue,
          count: metric.abnormalTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.watchdog.rawValue,
          count: metric.watchdogTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.cpuLimit.rawValue,
          count: metric.highCPUTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.memoryLimit.rawValue,
          count: metric.memoryLimitTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.memoryPressure.rawValue,
          count: metric.systemPressureTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.suspendedWithLockedFile.rawValue,
          count: metric.fileLockTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.badAccess.rawValue,
          count: metric.badAccessTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.illegalInstruction.rawValue,
          count: metric.illegalInstructionTerminationCount)
        HistogramBridge.reportExitReason(
          histogramName, withBucket: MetricKitExitReason.backgroundTaskAssertionTimeout.rawValue,
          count: metric.taskTimeoutTerminationCount)

      default:
        break
      }
    }
  }
#else
  // Stub implementation for Xcode/SDK < 27 or when compilation is not supported.
  @objc
  @MainActor
  public final class MetricKitReportSubscriber: NSObject {
    @objc
    public static let sharedInstance = MetricKitReportSubscriber()
    private override init() { super.init() }
    @objc
    public func setEnabled(_ enabled: Bool) {}
    @objc
    public func processReportForTesting(_ reportData: Data) {}
  }
#endif
