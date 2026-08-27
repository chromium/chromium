// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

// Public interfaces are exported and callable in all iOS builds. I.e. no need
// for guards at call sites.
//
// Core functionality is compiled away on Xcode/SDK < 27, and bypassed at
// runtime on iOS < 27.
//
// Note: All classes in this file are currently MainActor-isolated which is fine
// for now because all current users are on the Obj-C UI thread. However, this
// is not required for using the API. Consider making the classes non-isolated
// in the future, ideally without abusing `@unchecked Sendable`.

private let kDomainPrefix = "com.google.chrome.ios."

#if canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)
  import MetricKit
  import StateReporting
#endif  // canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)

@objc
public enum ActivityReportDomain: Int {
  case tab
  case ntp
  case signin
  case assistant
  case tabgrid
  case settings
  case test
  case testWithIncognito

  var stringValue: String {
    switch self {
    case .tab: return "tab"
    case .ntp: return "ntp"
    case .signin: return "signin"
    case .assistant: return "assistant"
    case .tabgrid: return "tabgrid"
    case .settings: return "settings"
    case .test: return "test"
    case .testWithIncognito: return "testWithIncognito"
    }
  }
}

struct EmptyMetadata: Sendable {}

#if canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)
  @available(iOS 27.0, *)
  extension EmptyMetadata: ReportableMetadata {
    var metadataDictionary: [String: ReportableMetadataValue] {
      return [:]
    }
  }
#endif  // canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)

struct IncognitoMetadata: Sendable {
  let incognito: Bool
}

#if canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)
  @available(iOS 27.0, *)
  extension IncognitoMetadata: ReportableMetadata {
    var metadataDictionary: [String: ReportableMetadataValue] {
      return [
        "incognito": ReportableMetadataValue(incognito)
      ]
    }
  }
#endif  // canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)

@MainActor
class ActivityReporterImplProtocol<S>: Sendable {
  init(domain: ActivityReportDomain, stableMetadata: S.Type) {}
  func reportActive(metadata: S) {}
  func reportInactive() {}
}

#if canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)
  @available(iOS 27.0, *)
  @MainActor
  private final class ActivityReporterImpl<S: ReportableMetadata>: ActivityReporterImplProtocol<S> {
    private let reporter: StateReporter<S, Never>

    override init(domain: ActivityReportDomain, stableMetadata: S.Type) {
      let fullDomainName = kDomainPrefix + domain.stringValue
      self.reporter = StateReporter.reporter(
        for: StateReportingDomain(rawValue: fullDomainName).rawValue,
        stableMetadata: stableMetadata
      )
      super.init(domain: domain, stableMetadata: stableMetadata)
    }

    override func reportActive(metadata: S) {
      reporter.reportTransition(to: "Active", stableMetadata: metadata)
    }

    override func reportInactive() {
      reporter.reportTransition(to: "Inactive")
    }
  }

  // Helper function to open the existential `ReportableMetadata.Type` and bind
  // it to a statically checked generic parameter `M: ReportableMetadata`.
  // This is required because MaybeActivityReporter's `S` generic parameter
  // cannot be defined as conforming to `ReportableMetadata` because the class
  // definition needs to be buildable with Xcode and SDK < 27.
  // TODO(crbug.com/523324356): cleanup once min Xcode and SDK version is 27.
  @available(iOS 27.0, *)
  @MainActor
  private func makeReporter<S, M: ReportableMetadata>(
    domain: ActivityReportDomain,
    stableMetadata: M.Type
  ) -> ActivityReporterImplProtocol<S> {
    return ActivityReporterImpl<M>(domain: domain, stableMetadata: stableMetadata)
      as! ActivityReporterImplProtocol<S>
  }
#endif  // canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)

@MainActor
private final class ActivityReporterImplStub<S>: ActivityReporterImplProtocol<S> {
  override init(domain: ActivityReportDomain, stableMetadata: S.Type) {
    super.init(domain: domain, stableMetadata: stableMetadata)
  }
  override func reportActive(metadata: S) {}
  override func reportInactive() {}
}

// Wrapper that decides whether to use the real implementation or the stub.
@MainActor
final class MaybeActivityReporter<S>: Sendable {
  private let reporter: ActivityReporterImplProtocol<S>

  init(domain: ActivityReportDomain, stableMetadata: S.Type) {
    #if canImport(StateReporting) && canImport(MetricKit) && compiler(>=6.4)
      if #available(iOS 27.0, *) {
        if let metadataType = S.self as? ReportableMetadata.Type {
          self.reporter = makeReporter(domain: domain, stableMetadata: metadataType)
          return
        }
      }
    #endif
    self.reporter = ActivityReporterImplStub<S>(
      domain: domain, stableMetadata: stableMetadata)
  }

  func reportActive(metadata: S) {
    reporter.reportActive(metadata: metadata)
  }

  func reportInactive() {
    reporter.reportInactive()
  }
}

// Objective-C interface for MetricKit state reporting without metadata.
@objc
@MainActor
public final class ActivityReporter: NSObject {
  private let internalReporter: MaybeActivityReporter<EmptyMetadata>

  @objc
  public init(domain: ActivityReportDomain) {
    self.internalReporter = MaybeActivityReporter<EmptyMetadata>(
      domain: domain, stableMetadata: EmptyMetadata.self)
  }

  @objc
  public func reportActive() {
    internalReporter.reportActive(metadata: EmptyMetadata())
  }

  @objc
  public func reportInactive() {
    internalReporter.reportInactive()
  }
}

// Objective-C interface for MetricKit state reporting with an incognito
// metadata field.
@objc
@MainActor
public final class ActivityReporterWithIncognito: NSObject {
  private let internalReporter: MaybeActivityReporter<IncognitoMetadata>

  @objc
  public init(domain: ActivityReportDomain) {
    self.internalReporter = MaybeActivityReporter<IncognitoMetadata>(
      domain: domain, stableMetadata: IncognitoMetadata.self)
  }

  @objc(reportActiveWithIncognito:)
  public func reportActive(incognito: Bool) {
    internalReporter.reportActive(metadata: IncognitoMetadata(incognito: incognito))
  }

  @objc
  public func reportInactive() {
    internalReporter.reportInactive()
  }
}
