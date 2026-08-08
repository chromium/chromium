// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_PRIVACY_FILTERED_FIELDS_INL_H_
#define SERVICES_TRACING_PERFETTO_PRIVACY_FILTERED_FIELDS_INL_H_

// This file is auto generated from internal copy of the TracePacket proto, that
// does not contain any privacy sensitive fields. Updates to this file should be
// made by changing internal copy and then running the generator script. Follow
// instructions at:
// https://goto.google.com/chrome-trace-privacy-filtered-fields

#include "base/memory/raw_ptr_exclusion.h"

namespace tracing {

// A MessageInfo node created from a tree of TracePacket proto messages.
struct MessageInfo {
  // List of accepted field ids in the output for this message. The end of list
  // is marked by a -1.
  // RAW_PTR_EXCLUSION: constant data that is not freed.
  RAW_PTR_EXCLUSION const int* accepted_field_ids;

  // List of sub messages that correspond to the accepted field ids list. There
  // is no end of list marker and the length is this list is equal to length of
  // |accepted_field_ids| - 1.
  // RAW_PTR_EXCLUSION: constant data that is not freed.
  RAW_PTR_EXCLUSION const MessageInfo* const* const sub_messages;
};

// Proto Message: Clock
inline constexpr int kClockIndices[] = {1, 2, 3, 4, -1};
inline constexpr MessageInfo kClock = {kClockIndices, nullptr};

// Proto Message: ClockSnapshot
inline constexpr int kClockSnapshotIndices[] = {1, 2, -1};
inline constexpr MessageInfo const* kClockSnapshotComplexMessages[] = {&kClock,
                                                                       nullptr};
inline constexpr MessageInfo kClockSnapshot = {kClockSnapshotIndices,
                                               kClockSnapshotComplexMessages};

// Proto Message: TaskExecution
inline constexpr int kTaskExecutionIndices[] = {1, -1};
inline constexpr MessageInfo kTaskExecution = {kTaskExecutionIndices, nullptr};

// Proto Message: LegacyEvent
inline constexpr int kLegacyEventIndices[] = {1,  2,  3,  4,  6,  8,  9, 10,
                                              11, 12, 13, 14, 18, 19, -1};
inline constexpr MessageInfo kLegacyEvent = {kLegacyEventIndices, nullptr};

// Proto Message: MajorState
inline constexpr int kMajorStateIndices[] = {1, 2, 3, 4, 5, -1};
inline constexpr MessageInfo kMajorState = {kMajorStateIndices, nullptr};

// Proto Message: MinorState
inline constexpr int kMinorStateIndices[] = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, -1};
inline constexpr MessageInfo kMinorState = {kMinorStateIndices, nullptr};

// Proto Message: ChromeCompositorStateMachine
inline constexpr int kChromeCompositorStateMachineIndices[] = {1, 2, -1};
inline constexpr MessageInfo const*
    kChromeCompositorStateMachineComplexMessages[] = {&kMajorState,
                                                      &kMinorState};
inline constexpr MessageInfo kChromeCompositorStateMachine = {
    kChromeCompositorStateMachineIndices,
    kChromeCompositorStateMachineComplexMessages};

// Proto Message: SourceLocation
inline constexpr int kSourceLocationIndices[] = {1, 2, 3, 4, -1};
inline constexpr MessageInfo kSourceLocation = {kSourceLocationIndices,
                                                nullptr};

// Proto Message: BeginFrameArgs
inline constexpr int kBeginFrameArgsIndices[] = {1, 2, 3, 4,  5, 6,
                                                 7, 8, 9, 10, -1};
inline constexpr MessageInfo const* kBeginFrameArgsComplexMessages[] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, &kSourceLocation};
inline constexpr MessageInfo kBeginFrameArgs = {kBeginFrameArgsIndices,
                                                kBeginFrameArgsComplexMessages};

// Proto Message: TimestampsInUs
inline constexpr int kTimestampsInUsIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
inline constexpr MessageInfo kTimestampsInUs = {kTimestampsInUsIndices,
                                                nullptr};

// Proto Message: BeginImplFrameArgs
inline constexpr int kBeginImplFrameArgsIndices[] = {1, 2, 3, 4, 5, 6, -1};
inline constexpr MessageInfo const* kBeginImplFrameArgsComplexMessages[] = {
    nullptr,          nullptr,          nullptr,
    &kBeginFrameArgs, &kBeginFrameArgs, &kTimestampsInUs};
inline constexpr MessageInfo kBeginImplFrameArgs = {
    kBeginImplFrameArgsIndices, kBeginImplFrameArgsComplexMessages};

// Proto Message: BeginFrameObserverState
inline constexpr int kBeginFrameObserverStateIndices[] = {1, 2, -1};
inline constexpr MessageInfo const* kBeginFrameObserverStateComplexMessages[] =
    {nullptr, &kBeginFrameArgs};
inline constexpr MessageInfo kBeginFrameObserverState = {
    kBeginFrameObserverStateIndices, kBeginFrameObserverStateComplexMessages};

// Proto Message: BeginFrameSourceState
inline constexpr int kBeginFrameSourceStateIndices[] = {1, 2, 3, 4, -1};
inline constexpr MessageInfo const* kBeginFrameSourceStateComplexMessages[] = {
    nullptr, nullptr, nullptr, &kBeginFrameArgs};
inline constexpr MessageInfo kBeginFrameSourceState = {
    kBeginFrameSourceStateIndices, kBeginFrameSourceStateComplexMessages};

// Proto Message: CompositorTimingHistory
inline constexpr int kCompositorTimingHistoryIndices[] = {1, 2, 3, 4,
                                                          5, 6, 7, -1};
inline constexpr MessageInfo kCompositorTimingHistory = {
    kCompositorTimingHistoryIndices, nullptr};

// Proto Message: ChromeCompositorSchedulerState
inline constexpr int kChromeCompositorSchedulerStateIndices[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, -1};
inline constexpr MessageInfo const*
    kChromeCompositorSchedulerStateComplexMessages[] = {
        &kChromeCompositorStateMachine,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &kBeginImplFrameArgs,
        &kBeginFrameObserverState,
        &kBeginFrameSourceState,
        &kCompositorTimingHistory};
inline constexpr MessageInfo kChromeCompositorSchedulerState = {
    kChromeCompositorSchedulerStateIndices,
    kChromeCompositorSchedulerStateComplexMessages};

// Proto Message: ChromeUserEvent
inline constexpr int kChromeUserEventIndices[] = {2, -1};
inline constexpr MessageInfo kChromeUserEvent = {kChromeUserEventIndices,
                                                 nullptr};

// Proto Message: ChromeKeyedService
inline constexpr int kChromeKeyedServiceIndices[] = {1, -1};
inline constexpr MessageInfo kChromeKeyedService = {kChromeKeyedServiceIndices,
                                                    nullptr};

// Proto Message: ChromeLegacyIpc
inline constexpr int kChromeLegacyIpcIndices[] = {1, 2, -1};
inline constexpr MessageInfo kChromeLegacyIpc = {kChromeLegacyIpcIndices,
                                                 nullptr};

// Proto Message: ChromeHistogramSample
inline constexpr int kChromeHistogramSampleIndices[] = {1, 3, -1};
inline constexpr MessageInfo kChromeHistogramSample = {
    kChromeHistogramSampleIndices, nullptr};

// Proto Message: ChromeFrameReporter
inline constexpr int kChromeFrameReporterIndices[] = {1, 2, 3,  4,  5,  6,  7,
                                                      8, 9, 10, 11, 17, 18, -1};
inline constexpr MessageInfo kChromeFrameReporter = {
    kChromeFrameReporterIndices, nullptr};

// Proto Message: ChromeMessagePump
inline constexpr int kChromeMessagePumpIndices[] = {1, 2, -1};
inline constexpr MessageInfo kChromeMessagePump = {kChromeMessagePumpIndices,
                                                   nullptr};

// Proto Message: ChromeMojoEventInfo
inline constexpr int kChromeMojoEventInfoIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
inline constexpr MessageInfo kChromeMojoEventInfo = {
    kChromeMojoEventInfoIndices, nullptr};

// Proto Message: ChromeApplicationStateInfo
inline constexpr int kChromeApplicationStateInfoIndices[] = {1, -1};
inline constexpr MessageInfo kChromeApplicationStateInfo = {
    kChromeApplicationStateInfoIndices, nullptr};

// Proto Message: ChromeRendererSchedulerState
inline constexpr int kChromeRendererSchedulerStateIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kChromeRendererSchedulerState = {
    kChromeRendererSchedulerStateIndices, nullptr};

// Proto Message: ChromeWindowHandleEventInfo
inline constexpr int kChromeWindowHandleEventInfoIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kChromeWindowHandleEventInfo = {
    kChromeWindowHandleEventInfoIndices, nullptr};

// Proto Message: ChromeContentSettingsEventInfo
inline constexpr int kChromeContentSettingsEventInfoIndices[] = {1, -1};
inline constexpr MessageInfo kChromeContentSettingsEventInfo = {
    kChromeContentSettingsEventInfoIndices, nullptr};

// Proto Message: ChromeMemoryPressureNotification
inline constexpr int kChromeMemoryPressureNotificationIndices[] = {1, 2, -1};
inline constexpr MessageInfo kChromeMemoryPressureNotification = {
    kChromeMemoryPressureNotificationIndices, nullptr};

// Proto Message: ChromeTaskAnnotator
inline constexpr int kChromeTaskAnnotatorIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kChromeTaskAnnotator = {
    kChromeTaskAnnotatorIndices, nullptr};

// Proto Message: ChromeBrowserContext
inline constexpr int kChromeBrowserContextIndices[] = {1, 2, -1};
inline constexpr MessageInfo kChromeBrowserContext = {
    kChromeBrowserContextIndices, nullptr};

// Proto Message: ChromeProfileDestroyer
inline constexpr int kChromeProfileDestroyerIndices[] = {1, 2, 4, 5, 6, -1};
inline constexpr MessageInfo kChromeProfileDestroyer = {
    kChromeProfileDestroyerIndices, nullptr};

// Proto Message: ChromeTaskPostedToDisabledQueue
inline constexpr int kChromeTaskPostedToDisabledQueueIndices[] = {2, 3, 4, -1};
inline constexpr MessageInfo kChromeTaskPostedToDisabledQueue = {
    kChromeTaskPostedToDisabledQueueIndices, nullptr};

// Proto Message: ChromeTaskGraphRunner
inline constexpr int kChromeTaskGraphRunnerIndices[] = {1, -1};
inline constexpr MessageInfo kChromeTaskGraphRunner = {
    kChromeTaskGraphRunnerIndices, nullptr};

// Proto Message: ChromeMessagePumpForUI
inline constexpr int kChromeMessagePumpForUIIndices[] = {1, 2, -1};
inline constexpr MessageInfo kChromeMessagePumpForUI = {
    kChromeMessagePumpForUIIndices, nullptr};

// Proto Message: RenderFrameImplDeletion
inline constexpr int kRenderFrameImplDeletionIndices[] = {1, 2, 3, 4, -1};
inline constexpr MessageInfo kRenderFrameImplDeletion = {
    kRenderFrameImplDeletionIndices, nullptr};

// Proto Message: ShouldSwapBrowsingInstancesResult
inline constexpr int kShouldSwapBrowsingInstancesResultIndices[] = {1, 2, -1};
inline constexpr MessageInfo kShouldSwapBrowsingInstancesResult = {
    kShouldSwapBrowsingInstancesResultIndices, nullptr};

// Proto Message: FrameTreeNodeInfo
inline constexpr int kFrameTreeNodeInfoIndices[] = {1, 2, 3, 6, -1};
inline constexpr MessageInfo kFrameTreeNodeInfo = {kFrameTreeNodeInfoIndices,
                                                   nullptr};

// Proto Message: ChromeHashedPerformanceMark
inline constexpr int kChromeHashedPerformanceMarkIndices[] = {1, 3, 5, 6, -1};
inline constexpr MessageInfo kChromeHashedPerformanceMark = {
    kChromeHashedPerformanceMarkIndices, nullptr};

// Proto Message: RenderProcessHost
inline constexpr int kRenderProcessHostIndices[] = {1, 3, 4, -1};
inline constexpr MessageInfo const* kRenderProcessHostComplexMessages[] = {
    nullptr, nullptr, &kChromeBrowserContext};
inline constexpr MessageInfo kRenderProcessHost = {
    kRenderProcessHostIndices, kRenderProcessHostComplexMessages};

// Proto Message: RenderProcessHostCleanup
inline constexpr int kRenderProcessHostCleanupIndices[] = {1, 2, 3, 4, 5, -1};
inline constexpr MessageInfo kRenderProcessHostCleanup = {
    kRenderProcessHostCleanupIndices, nullptr};

// Proto Message: RenderProcessHostListener
inline constexpr int kRenderProcessHostListenerIndices[] = {1, -1};
inline constexpr MessageInfo kRenderProcessHostListener = {
    kRenderProcessHostListenerIndices, nullptr};

// Proto Message: ChildProcessLauncherPriority
inline constexpr int kChildProcessLauncherPriorityIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kChildProcessLauncherPriority = {
    kChildProcessLauncherPriorityIndices, nullptr};

// Proto Message: ResourceBundle
inline constexpr int kResourceBundleIndices[] = {1, -1};
inline constexpr MessageInfo kResourceBundle = {kResourceBundleIndices,
                                                nullptr};

// Proto Message: ChromeWebAppBadNavigate
inline constexpr int kChromeWebAppBadNavigateIndices[] = {1, 2, 4, 5, 6, -1};
inline constexpr MessageInfo kChromeWebAppBadNavigate = {
    kChromeWebAppBadNavigateIndices, nullptr};

// Proto Message: ChromeExtensionId
inline constexpr int kChromeExtensionIdIndices[] = {2, -1};
inline constexpr MessageInfo kChromeExtensionId = {kChromeExtensionIdIndices,
                                                   nullptr};

// Proto Message: SiteInstanceGroup
inline constexpr int kSiteInstanceGroupIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo const* kSiteInstanceGroupComplexMessages[] = {
    nullptr, nullptr, &kRenderProcessHost};
inline constexpr MessageInfo kSiteInstanceGroup = {
    kSiteInstanceGroupIndices, kSiteInstanceGroupComplexMessages};

// Proto Message: SiteInstance
inline constexpr int kSiteInstanceIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
inline constexpr MessageInfo const* kSiteInstanceComplexMessages[] = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &kSiteInstanceGroup};
inline constexpr MessageInfo kSiteInstance = {kSiteInstanceIndices,
                                              kSiteInstanceComplexMessages};

// Proto Message: RenderViewHost
inline constexpr int kRenderViewHostIndices[] = {1, 2, 3, 4, 5, -1};
inline constexpr MessageInfo kRenderViewHost = {kRenderViewHostIndices,
                                                nullptr};

// Proto Message: RenderFrameProxyHost
inline constexpr int kRenderFrameProxyHostIndices[] = {1, 2, 3, 4, 5, 6, -1};
inline constexpr MessageInfo kRenderFrameProxyHost = {
    kRenderFrameProxyHostIndices, nullptr};

// Proto Message: AndroidView
inline constexpr int kAndroidViewIndices[] = {1, 2, 3, 4, 5, 6, -1};
inline constexpr MessageInfo kAndroidView = {kAndroidViewIndices, nullptr};

// Proto Message: AndroidActivity
inline constexpr int kAndroidActivityIndices[] = {1, 2, -1};
inline constexpr MessageInfo const* kAndroidActivityComplexMessages[] = {
    nullptr, &kAndroidView};
inline constexpr MessageInfo kAndroidActivity = {
    kAndroidActivityIndices, kAndroidActivityComplexMessages};

// Proto Message: AndroidViewDump
inline constexpr int kAndroidViewDumpIndices[] = {1, -1};
inline constexpr MessageInfo const* kAndroidViewDumpComplexMessages[] = {
    &kAndroidActivity};
inline constexpr MessageInfo kAndroidViewDump = {
    kAndroidViewDumpIndices, kAndroidViewDumpComplexMessages};

// Proto Message: ParkableStringCompressInBackground
inline constexpr int kParkableStringCompressInBackgroundIndices[] = {1, -1};
inline constexpr MessageInfo kParkableStringCompressInBackground = {
    kParkableStringCompressInBackgroundIndices, nullptr};

// Proto Message: ParkableStringUnpark
inline constexpr int kParkableStringUnparkIndices[] = {1, 2, -1};
inline constexpr MessageInfo kParkableStringUnpark = {
    kParkableStringUnparkIndices, nullptr};

// Proto Message: ChromeSamplingProfilerSampleCollected
inline constexpr int kChromeSamplingProfilerSampleCollectedIndices[] = {1, 2, 3,
                                                                        -1};
inline constexpr MessageInfo kChromeSamplingProfilerSampleCollected = {
    kChromeSamplingProfilerSampleCollectedIndices, nullptr};

// Proto Message: RenderFrameHost
inline constexpr int kRenderFrameHostIndices[] = {3, 6, 12, -1};
inline constexpr MessageInfo kRenderFrameHost = {kRenderFrameHostIndices,
                                                 nullptr};

// Proto Message: RendererMainThreadTaskExecution
inline constexpr int kRendererMainThreadTaskExecutionIndices[] = {1, 2, 3,
                                                                  4, 5, -1};
inline constexpr MessageInfo kRendererMainThreadTaskExecution = {
    kRendererMainThreadTaskExecutionIndices, nullptr};

// Proto Message: MissedVsyncsForJankReason
inline constexpr int kMissedVsyncsForJankReasonIndices[] = {1, 2, -1};
inline constexpr MessageInfo kMissedVsyncsForJankReason = {
    kMissedVsyncsForJankReasonIndices, nullptr};

// Proto Message: Real
inline constexpr int kRealIndices[] = {1, 2, 3, 4, -1};
inline constexpr MessageInfo kReal = {kRealIndices, nullptr};

// Proto Message: Synthetic
inline constexpr int kSyntheticIndices[] = {1, 2, -1};
inline constexpr MessageInfo kSynthetic = {kSyntheticIndices, nullptr};

// Proto Message: ScrollUpdates
inline constexpr int kScrollUpdatesIndices[] = {1, 2, 3, 4, -1};
inline constexpr MessageInfo const* kScrollUpdatesComplexMessages[] = {
    &kReal, &kSynthetic, nullptr, nullptr};
inline constexpr MessageInfo kScrollUpdates = {kScrollUpdatesIndices,
                                               kScrollUpdatesComplexMessages};

// Proto Message: FrameStageCalculation
inline constexpr int kFrameStageCalculationIndices[] = {1, 2, -1};
inline constexpr MessageInfo kFrameStageCalculation = {
    kFrameStageCalculationIndices, nullptr};

// Proto Message: ScrollJankV4Result
inline constexpr int kScrollJankV4ResultIndices[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, -1};
inline constexpr MessageInfo const* kScrollJankV4ResultComplexMessages[] = {
    nullptr, &kMissedVsyncsForJankReason,
    nullptr, nullptr,
    nullptr, nullptr,
    nullptr, nullptr,
    nullptr, &kScrollUpdates,
    nullptr, nullptr,
    nullptr, &kFrameStageCalculation};
inline constexpr MessageInfo kScrollJankV4Result = {
    kScrollJankV4ResultIndices, kScrollJankV4ResultComplexMessages};

// Proto Message: EventLatency
inline constexpr int kEventLatencyIndices[] = {1, 2, 4,  5,  6, 7,
                                               8, 9, 10, 11, -1};
inline constexpr MessageInfo const* kEventLatencyComplexMessages[] = {
    nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr,
    nullptr, nullptr, &kScrollJankV4Result,
    nullptr};
inline constexpr MessageInfo kEventLatency = {kEventLatencyIndices,
                                              kEventLatencyComplexMessages};

// Proto Message: ProcessSingleton
inline constexpr int kProcessSingletonIndices[] = {1, 2, -1};
inline constexpr MessageInfo kProcessSingleton = {kProcessSingletonIndices,
                                                  nullptr};

// Proto Message: AndroidIPC
inline constexpr int kAndroidIPCIndices[] = {1, 2, -1};
inline constexpr MessageInfo kAndroidIPC = {kAndroidIPCIndices, nullptr};

// Proto Message: ChromeSqlDiagnostics
inline constexpr int kChromeSqlDiagnosticsIndices[] = {1, 2, 3, 4,  5, 6,
                                                       7, 8, 9, 10, -1};
inline constexpr MessageInfo kChromeSqlDiagnostics = {
    kChromeSqlDiagnosticsIndices, nullptr};

// Proto Message: SequenceManagerTask
inline constexpr int kSequenceManagerTaskIndices[] = {1, 2, -1};
inline constexpr MessageInfo kSequenceManagerTask = {
    kSequenceManagerTaskIndices, nullptr};

// Proto Message: AndroidToolbar
inline constexpr int kAndroidToolbarIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kAndroidToolbar = {kAndroidToolbarIndices,
                                                nullptr};

// Proto Message: ActiveProcesses
inline constexpr int kActiveProcessesIndices[] = {1, -1};
inline constexpr MessageInfo kActiveProcesses = {kActiveProcessesIndices,
                                                 nullptr};

// Proto Message: TabSwitchMeasurement
inline constexpr int kTabSwitchMeasurementIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kTabSwitchMeasurement = {
    kTabSwitchMeasurementIndices, nullptr};

// Proto Message: ScrollDeltas
inline constexpr int kScrollDeltasIndices[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, -1};
inline constexpr MessageInfo kScrollDeltas = {kScrollDeltasIndices, nullptr};

// Proto Message: WinRenderAudioFromSource
inline constexpr int kWinRenderAudioFromSourceIndices[] = {1, 2, 3, 4,  5,  6,
                                                           7, 8, 9, 10, 11, -1};
inline constexpr MessageInfo kWinRenderAudioFromSource = {
    kWinRenderAudioFromSourceIndices, nullptr};

// Proto Message: MacAUHALStream
inline constexpr int kMacAUHALStreamIndices[] = {1, 2,  3,  4,  5,  6,  7, 8,
                                                 9, 10, 11, 12, 13, 14, -1};
inline constexpr MessageInfo kMacAUHALStream = {kMacAUHALStreamIndices,
                                                nullptr};

// Proto Message: LinuxAlsaOutput
inline constexpr int kLinuxAlsaOutputIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
inline constexpr MessageInfo kLinuxAlsaOutput = {kLinuxAlsaOutputIndices,
                                                 nullptr};

// Proto Message: LinuxPulseOutput
inline constexpr int kLinuxPulseOutputIndices[] = {1, 2, 3, 4, 5, -1};
inline constexpr MessageInfo kLinuxPulseOutput = {kLinuxPulseOutputIndices,
                                                  nullptr};

// Proto Message: FrameSinkId
inline constexpr int kFrameSinkIdIndices[] = {1, 2, -1};
inline constexpr MessageInfo kFrameSinkId = {kFrameSinkIdIndices, nullptr};

// Proto Message: ChromeUnguessableToken
inline constexpr int kChromeUnguessableTokenIndices[] = {1, 2, -1};
inline constexpr MessageInfo kChromeUnguessableToken = {
    kChromeUnguessableTokenIndices, nullptr};

// Proto Message: LocalSurfaceId
inline constexpr int kLocalSurfaceIdIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo const* kLocalSurfaceIdComplexMessages[] = {
    nullptr, nullptr, &kChromeUnguessableToken};
inline constexpr MessageInfo kLocalSurfaceId = {kLocalSurfaceIdIndices,
                                                kLocalSurfaceIdComplexMessages};

// Proto Message: ChromeGraphicsPipeline
inline constexpr int kChromeGraphicsPipelineIndices[] = {1, 2, 3, 4,  5,
                                                         6, 8, 9, 10, -1};
inline constexpr MessageInfo const* kChromeGraphicsPipelineComplexMessages[] = {
    nullptr, &kFrameSinkId, nullptr, &kLocalSurfaceId, nullptr,
    nullptr, nullptr,       nullptr, nullptr};
inline constexpr MessageInfo kChromeGraphicsPipeline = {
    kChromeGraphicsPipelineIndices, kChromeGraphicsPipelineComplexMessages};

// Proto Message: CrasUnified
inline constexpr int kCrasUnifiedIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
inline constexpr MessageInfo kCrasUnified = {kCrasUnifiedIndices, nullptr};

// Proto Message: LibunwindstackUnwinder
inline constexpr int kLibunwindstackUnwinderIndices[] = {1, 2, -1};
inline constexpr MessageInfo kLibunwindstackUnwinder = {
    kLibunwindstackUnwinderIndices, nullptr};

// Proto Message: EventFrameValue
inline constexpr int kEventFrameValueIndices[] = {1, 2, -1};
inline constexpr MessageInfo kEventFrameValue = {kEventFrameValueIndices,
                                                 nullptr};

// Proto Message: ScrollPredictorMetrics
inline constexpr int kScrollPredictorMetricsIndices[] = {1, 2, 3, 4, 5, 6, -1};
inline constexpr MessageInfo const* kScrollPredictorMetricsComplexMessages[] = {
    &kEventFrameValue, &kEventFrameValue, &kEventFrameValue,
    nullptr,           nullptr,           nullptr};
inline constexpr MessageInfo kScrollPredictorMetrics = {
    kScrollPredictorMetricsIndices, kScrollPredictorMetricsComplexMessages};

// Proto Message: PageLoad
inline constexpr int kPageLoadIndices[] = {1, -1};
inline constexpr MessageInfo kPageLoad = {kPageLoadIndices, nullptr};

// Proto Message: StartUp
inline constexpr int kStartUpIndices[] = {1, 3, -1};
inline constexpr MessageInfo kStartUp = {kStartUpIndices, nullptr};

// Proto Message: WebContentInteraction
inline constexpr int kWebContentInteractionIndices[] = {1, 2, -1};
inline constexpr MessageInfo kWebContentInteraction = {
    kWebContentInteractionIndices, nullptr};

// Proto Message: EventForwarder
inline constexpr int kEventForwarderIndices[] = {1, 2, 5, 6, 7, 8, 9, 10, -1};
inline constexpr MessageInfo kEventForwarder = {kEventForwarderIndices,
                                                nullptr};

// Proto Message: TouchDispositionGestureFilter
inline constexpr int kTouchDispositionGestureFilterIndices[] = {1, -1};
inline constexpr MessageInfo kTouchDispositionGestureFilter = {
    kTouchDispositionGestureFilterIndices, nullptr};

// Proto Message: ViewClassName
inline constexpr int kViewClassNameIndices[] = {1, -1};
inline constexpr MessageInfo kViewClassName = {kViewClassNameIndices, nullptr};

// Proto Message: AnimationFrameTimingInfo
inline constexpr int kAnimationFrameTimingInfoIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kAnimationFrameTimingInfo = {
    kAnimationFrameTimingInfoIndices, nullptr};

// Proto Message: AnimationFrameScriptTimingInfo
inline constexpr int kAnimationFrameScriptTimingInfoIndices[] = {1, 2,  3,
                                                                 9, 10, -1};
inline constexpr MessageInfo kAnimationFrameScriptTimingInfo = {
    kAnimationFrameScriptTimingInfoIndices, nullptr};

// Proto Message: ScrollMetrics
inline constexpr int kScrollMetricsIndices[] = {1, 2, 3, 4, 5, 6, -1};
inline constexpr MessageInfo kScrollMetrics = {kScrollMetricsIndices, nullptr};

// Proto Message: BeginFrameId
inline constexpr int kBeginFrameIdIndices[] = {1, 2, -1};
inline constexpr MessageInfo kBeginFrameId = {kBeginFrameIdIndices, nullptr};

// Proto Message: MainFramePipeline
inline constexpr int kMainFramePipelineIndices[] = {1, 2, 3, 4, 5, -1};
inline constexpr MessageInfo const* kMainFramePipelineComplexMessages[] = {
    nullptr, nullptr, &kBeginFrameId, nullptr, &kBeginFrameId};
inline constexpr MessageInfo kMainFramePipeline = {
    kMainFramePipelineIndices, kMainFramePipelineComplexMessages};

// Proto Message: ComponentInfo
inline constexpr int kComponentInfoIndices[] = {1, 2, -1};
inline constexpr MessageInfo kComponentInfo = {kComponentInfoIndices, nullptr};

// Proto Message: ChromeLatencyInfo2
inline constexpr int kChromeLatencyInfo2Indices[] = {1, 2, 3, 4,  5, 6,
                                                     7, 8, 9, 10, -1};
inline constexpr MessageInfo const* kChromeLatencyInfo2ComplexMessages[] = {
    nullptr, nullptr, nullptr, &kComponentInfo, nullptr,
    nullptr, nullptr, nullptr, nullptr,         nullptr};
inline constexpr MessageInfo kChromeLatencyInfo2 = {
    kChromeLatencyInfo2Indices, kChromeLatencyInfo2ComplexMessages};

// Proto Message: EventTiming
inline constexpr int kEventTimingIndices[] = {1, 3, 4, 5, 6, 7, 8, 9, -1};
inline constexpr MessageInfo kEventTiming = {kEventTimingIndices, nullptr};

// Proto Message: FrameTimeline
inline constexpr int kFrameTimelineIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kFrameTimeline = {kFrameTimelineIndices, nullptr};

// Proto Message: AndroidChoreographerFrameCallbackData
inline constexpr int kAndroidChoreographerFrameCallbackDataIndices[] = {1, 2, 3,
                                                                        4, -1};
inline constexpr MessageInfo const*
    kAndroidChoreographerFrameCallbackDataComplexMessages[] = {
        nullptr, &kFrameTimeline, nullptr, &kFrameTimeline};
inline constexpr MessageInfo kAndroidChoreographerFrameCallbackData = {
    kAndroidChoreographerFrameCallbackDataIndices,
    kAndroidChoreographerFrameCallbackDataComplexMessages};

// Proto Message: CurrentTask
inline constexpr int kCurrentTaskIndices[] = {1, 2, -1};
inline constexpr MessageInfo kCurrentTask = {kCurrentTaskIndices, nullptr};

// Proto Message: ChromeFrameReporter2
inline constexpr int kChromeFrameReporter2Indices[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 17, 18, -1};
inline constexpr MessageInfo kChromeFrameReporter2 = {
    kChromeFrameReporter2Indices, nullptr};

// Proto Message: TopControlsOffset
inline constexpr int kTopControlsOffsetIndices[] = {1, 2, -1};
inline constexpr MessageInfo kTopControlsOffset = {kTopControlsOffsetIndices,
                                                   nullptr};

// Proto Message: InputTransferHandler
inline constexpr int kInputTransferHandlerIndices[] = {1, 2, 3, 4, -1};
inline constexpr MessageInfo const* kInputTransferHandlerComplexMessages[] = {
    nullptr, nullptr, nullptr, &kTopControlsOffset};
inline constexpr MessageInfo kInputTransferHandler = {
    kInputTransferHandlerIndices, kInputTransferHandlerComplexMessages};

// Proto Message: ResponseInfo
inline constexpr int kResponseInfoIndices[] = {1, 2, -1};
inline constexpr MessageInfo kResponseInfo = {kResponseInfoIndices, nullptr};

// Proto Message: MemoryDumpProvider
inline constexpr int kMemoryDumpProviderIndices[] = {1, -1};
inline constexpr MessageInfo kMemoryDumpProvider = {kMemoryDumpProviderIndices,
                                                    nullptr};

// Proto Message: ChromeAccessibilityWinNotifyWinEvent
inline constexpr int kChromeAccessibilityWinNotifyWinEventIndices[] = {1, -1};
inline constexpr MessageInfo kChromeAccessibilityWinNotifyWinEvent = {
    kChromeAccessibilityWinNotifyWinEventIndices, nullptr};

// Proto Message: ResultInterval
inline constexpr int kResultIntervalIndices[] = {1, 2, -1};
inline constexpr MessageInfo kResultInterval = {kResultIntervalIndices,
                                                nullptr};

// Proto Message: Result
inline constexpr int kResultIndices[] = {1, 2, -1};
inline constexpr MessageInfo const* kResultComplexMessages[] = {
    nullptr, &kResultInterval};
inline constexpr MessageInfo kResult = {kResultIndices, kResultComplexMessages};

// Proto Message: FrameIntervalDecider
inline constexpr int kFrameIntervalDeciderIndices[] = {1, 2, -1};
inline constexpr MessageInfo const* kFrameIntervalDeciderComplexMessages[] = {
    &kResult, nullptr};
inline constexpr MessageInfo kFrameIntervalDecider = {
    kFrameIntervalDeciderIndices, kFrameIntervalDeciderComplexMessages};

// Proto Message: BeginFrameArgsV2
inline constexpr int kBeginFrameArgsV2Indices[] = {4, 6, -1};
inline constexpr MessageInfo kBeginFrameArgsV2 = {kBeginFrameArgsV2Indices,
                                                  nullptr};

// Proto Message: MacVoucherRelease
inline constexpr int kMacVoucherReleaseIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
inline constexpr MessageInfo kMacVoucherRelease = {kMacVoucherReleaseIndices,
                                                   nullptr};

// Proto Message: AndroidVsyncIntervalDecision
inline constexpr int kAndroidVsyncIntervalDecisionIndices[] = {1, 2, 3,
                                                               4, 5, -1};
inline constexpr MessageInfo kAndroidVsyncIntervalDecision = {
    kAndroidVsyncIntervalDecisionIndices, nullptr};

// Proto Message: FrameDeadlineDecider
inline constexpr int kFrameDeadlineDeciderIndices[] = {1, 2, -1};
inline constexpr MessageInfo const* kFrameDeadlineDeciderComplexMessages[] = {
    &kFrameTimeline, nullptr};
inline constexpr MessageInfo kFrameDeadlineDecider = {
    kFrameDeadlineDeciderIndices, kFrameDeadlineDeciderComplexMessages};

// Proto Message: TrackEvent
inline constexpr int kTrackEventIndices[] = {
    1,    2,    3,    5,    6,    9,    10,   11,   12,   16,   17,   22,
    23,   24,   25,   26,   27,   28,   30,   31,   32,   33,   34,   35,
    36,   38,   39,   40,   41,   42,   43,   44,   47,   48,   1001, 1002,
    1003, 1004, 1005, 1006, 1007, 1008, 1009, 1010, 1011, 1012, 1013, 1014,
    1015, 1016, 1017, 1018, 1019, 1020, 1021, 1022, 1023, 1024, 1025, 1028,
    1031, 1032, 1033, 1034, 1036, 1038, 1039, 1040, 1041, 1042, 1046, 1047,
    1048, 1049, 1050, 1051, 1052, 1053, 1054, 1055, 1056, 1057, 1058, 1059,
    1060, 1061, 1064, 1065, 1066, 1067, 1068, 1069, 1070, 1071, 1075, 1076,
    1077, 1078, 1079, 1080, 1081, 1082, 1083, 1084, 1085, -1};
inline constexpr MessageInfo const* kTrackEventComplexMessages[] = {
    nullptr,
    nullptr,
    nullptr,
    &kTaskExecution,
    &kLegacyEvent,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &kChromeCompositorSchedulerState,
    &kChromeUserEvent,
    &kChromeKeyedService,
    &kChromeLegacyIpc,
    &kChromeHistogramSample,
    nullptr,
    nullptr,
    &kChromeFrameReporter,
    &kSourceLocation,
    nullptr,
    &kChromeMessagePump,
    nullptr,
    &kChromeMojoEventInfo,
    &kChromeApplicationStateInfo,
    &kChromeRendererSchedulerState,
    &kChromeWindowHandleEventInfo,
    nullptr,
    &kChromeContentSettingsEventInfo,
    nullptr,
    nullptr,
    nullptr,
    &kChromeMemoryPressureNotification,
    &kChromeTaskAnnotator,
    &kChromeBrowserContext,
    &kChromeProfileDestroyer,
    &kChromeTaskPostedToDisabledQueue,
    &kChromeTaskGraphRunner,
    &kChromeMessagePumpForUI,
    &kRenderFrameImplDeletion,
    &kShouldSwapBrowsingInstancesResult,
    &kFrameTreeNodeInfo,
    &kChromeHashedPerformanceMark,
    &kRenderProcessHost,
    &kRenderProcessHostCleanup,
    &kRenderProcessHostListener,
    &kChildProcessLauncherPriority,
    &kResourceBundle,
    &kChromeWebAppBadNavigate,
    &kChromeExtensionId,
    &kSiteInstance,
    &kRenderViewHost,
    &kRenderFrameProxyHost,
    &kAndroidViewDump,
    &kParkableStringCompressInBackground,
    &kParkableStringUnpark,
    &kChromeSamplingProfilerSampleCollected,
    &kRenderFrameHost,
    &kRendererMainThreadTaskExecution,
    &kEventLatency,
    &kProcessSingleton,
    &kSiteInstanceGroup,
    nullptr,
    &kAndroidIPC,
    &kChromeSqlDiagnostics,
    &kSequenceManagerTask,
    &kAndroidToolbar,
    &kActiveProcesses,
    &kTabSwitchMeasurement,
    &kScrollDeltas,
    &kWinRenderAudioFromSource,
    &kMacAUHALStream,
    &kLinuxAlsaOutput,
    &kLinuxPulseOutput,
    &kChromeGraphicsPipeline,
    &kCrasUnified,
    &kLibunwindstackUnwinder,
    &kScrollPredictorMetrics,
    &kPageLoad,
    &kStartUp,
    &kWebContentInteraction,
    &kEventForwarder,
    &kTouchDispositionGestureFilter,
    &kViewClassName,
    &kAnimationFrameTimingInfo,
    &kAnimationFrameScriptTimingInfo,
    &kScrollMetrics,
    &kMainFramePipeline,
    &kChromeLatencyInfo2,
    &kEventTiming,
    &kAndroidChoreographerFrameCallbackData,
    &kCurrentTask,
    &kChromeFrameReporter2,
    &kInputTransferHandler,
    &kResponseInfo,
    &kScrollJankV4Result,
    &kMemoryDumpProvider,
    &kChromeAccessibilityWinNotifyWinEvent,
    &kFrameIntervalDecider,
    &kBeginFrameArgsV2,
    &kMacVoucherRelease,
    &kAndroidVsyncIntervalDecision,
    &kFrameDeadlineDecider};
inline constexpr MessageInfo kTrackEvent = {kTrackEventIndices,
                                            kTrackEventComplexMessages};

// Proto Message: EventCategory
inline constexpr int kEventCategoryIndices[] = {1, 2, -1};
inline constexpr MessageInfo kEventCategory = {kEventCategoryIndices, nullptr};

// Proto Message: EventName
inline constexpr int kEventNameIndices[] = {1, 2, -1};
inline constexpr MessageInfo kEventName = {kEventNameIndices, nullptr};

// Proto Message: InternedString
inline constexpr int kInternedStringIndices[] = {1, 2, -1};
inline constexpr MessageInfo kInternedString = {kInternedStringIndices,
                                                nullptr};

// Proto Message: Frame
inline constexpr int kFrameIndices[] = {1, 2, 3, 4, -1};
inline constexpr MessageInfo kFrame = {kFrameIndices, nullptr};

// Proto Message: Callstack
inline constexpr int kCallstackIndices[] = {1, 2, -1};
inline constexpr MessageInfo kCallstack = {kCallstackIndices, nullptr};

// Proto Message: InternedBuildId
inline constexpr int kInternedBuildIdIndices[] = {1, 2, -1};
inline constexpr MessageInfo kInternedBuildId = {kInternedBuildIdIndices,
                                                 nullptr};

// Proto Message: InternedMappingPath
inline constexpr int kInternedMappingPathIndices[] = {1, 2, -1};
inline constexpr MessageInfo kInternedMappingPath = {
    kInternedMappingPathIndices, nullptr};

// Proto Message: Mapping
inline constexpr int kMappingIndices[] = {1, 2, 3, 4, 5, 7, -1};
inline constexpr MessageInfo kMapping = {kMappingIndices, nullptr};

// Proto Message: UnsymbolizedSourceLocation
inline constexpr int kUnsymbolizedSourceLocationIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kUnsymbolizedSourceLocation = {
    kUnsymbolizedSourceLocationIndices, nullptr};

// Proto Message: InternedData
inline constexpr int kInternedDataIndices[] = {1,  2,  4,  5,  6, 7,
                                               16, 17, 19, 28, -1};
inline constexpr MessageInfo const* kInternedDataComplexMessages[] = {
    &kEventCategory,   &kEventName,
    &kSourceLocation,  &kInternedString,
    &kFrame,           &kCallstack,
    &kInternedBuildId, &kInternedMappingPath,
    &kMapping,         &kUnsymbolizedSourceLocation};
inline constexpr MessageInfo kInternedData = {kInternedDataIndices,
                                              kInternedDataComplexMessages};

// Proto Message: BufferStats
inline constexpr int kBufferStatsIndices[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, -1};
inline constexpr MessageInfo kBufferStats = {kBufferStatsIndices, nullptr};

// Proto Message: TraceStats
inline constexpr int kTraceStatsIndices[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1};
inline constexpr MessageInfo const* kTraceStatsComplexMessages[] = {
    &kBufferStats, nullptr, nullptr, nullptr, nullptr,
    nullptr,       nullptr, nullptr, nullptr, nullptr};
inline constexpr MessageInfo kTraceStats = {kTraceStatsIndices,
                                            kTraceStatsComplexMessages};

// Proto Message: ProcessDescriptor
inline constexpr int kProcessDescriptorIndices[] = {1, 4, 5, 7, -1};
inline constexpr MessageInfo kProcessDescriptor = {kProcessDescriptorIndices,
                                                   nullptr};

// Proto Message: ThreadDescriptor
inline constexpr int kThreadDescriptorIndices[] = {1, 2, 4, 6, 7, -1};
inline constexpr MessageInfo kThreadDescriptor = {kThreadDescriptorIndices,
                                                  nullptr};

// Proto Message: HistogramRule
inline constexpr int kHistogramRuleIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kHistogramRule = {kHistogramRuleIndices, nullptr};

// Proto Message: NamedRule
inline constexpr int kNamedRuleIndices[] = {1, 2, -1};
inline constexpr MessageInfo kNamedRule = {kNamedRuleIndices, nullptr};

// Proto Message: TriggerRule
inline constexpr int kTriggerRuleIndices[] = {1, 2, 3, 4, -1};
inline constexpr MessageInfo const* kTriggerRuleComplexMessages[] = {
    nullptr, &kHistogramRule, &kNamedRule, nullptr};
inline constexpr MessageInfo kTriggerRule = {kTriggerRuleIndices,
                                             kTriggerRuleComplexMessages};

// Proto Message: BackgroundTracingMetadata
inline constexpr int kBackgroundTracingMetadataIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo const*
    kBackgroundTracingMetadataComplexMessages[] = {&kTriggerRule, &kTriggerRule,
                                                   nullptr};
inline constexpr MessageInfo kBackgroundTracingMetadata = {
    kBackgroundTracingMetadataIndices,
    kBackgroundTracingMetadataComplexMessages};

// Proto Message: FinchHash
inline constexpr int kFinchHashIndices[] = {1, 2, -1};
inline constexpr MessageInfo kFinchHash = {kFinchHashIndices, nullptr};

// Proto Message: ChromeMetadataPacket
inline constexpr int kChromeMetadataPacketIndices[] = {1, 2, 3, 4, 6, 7, 8, -1};
inline constexpr MessageInfo const* kChromeMetadataPacketComplexMessages[] = {
    &kBackgroundTracingMetadata,
    nullptr,
    nullptr,
    &kFinchHash,
    nullptr,
    nullptr,
    nullptr};
inline constexpr MessageInfo kChromeMetadataPacket = {
    kChromeMetadataPacketIndices, kChromeMetadataPacketComplexMessages};

// Proto Message: StreamingProfilePacket
inline constexpr int kStreamingProfilePacketIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kStreamingProfilePacket = {
    kStreamingProfilePacketIndices, nullptr};

// Proto Message: HeapGraphObject
inline constexpr int kHeapGraphObjectIndices[] = {1, 2, 3, 4, 5, -1};
inline constexpr MessageInfo kHeapGraphObject = {kHeapGraphObjectIndices,
                                                 nullptr};

// Proto Message: InternedHeapGraphObjectTypes
inline constexpr int kInternedHeapGraphObjectTypesIndices[] = {1, 2, -1};
inline constexpr MessageInfo kInternedHeapGraphObjectTypes = {
    kInternedHeapGraphObjectTypesIndices, nullptr};

// Proto Message: InternedHeapGraphReferenceFieldNames
inline constexpr int kInternedHeapGraphReferenceFieldNamesIndices[] = {1, 2,
                                                                       -1};
inline constexpr MessageInfo kInternedHeapGraphReferenceFieldNames = {
    kInternedHeapGraphReferenceFieldNamesIndices, nullptr};

// Proto Message: HeapGraph
inline constexpr int kHeapGraphIndices[] = {1, 2, 3, 4, 5, 6, -1};
inline constexpr MessageInfo const* kHeapGraphComplexMessages[] = {
    nullptr,
    &kHeapGraphObject,
    &kInternedHeapGraphObjectTypes,
    &kInternedHeapGraphReferenceFieldNames,
    nullptr,
    nullptr};
inline constexpr MessageInfo kHeapGraph = {kHeapGraphIndices,
                                           kHeapGraphComplexMessages};

// Proto Message: TrackEventDefaults
inline constexpr int kTrackEventDefaultsIndices[] = {11, 31, -1};
inline constexpr MessageInfo kTrackEventDefaults = {kTrackEventDefaultsIndices,
                                                    nullptr};

// Proto Message: TracePacketDefaults
inline constexpr int kTracePacketDefaultsIndices[] = {11, 58, -1};
inline constexpr MessageInfo const* kTracePacketDefaultsComplexMessages[] = {
    &kTrackEventDefaults, nullptr};
inline constexpr MessageInfo kTracePacketDefaults = {
    kTracePacketDefaultsIndices, kTracePacketDefaultsComplexMessages};

// Proto Message: ChromeProcessDescriptor
inline constexpr int kChromeProcessDescriptorIndices[] = {1, 2, 3, 5, -1};
inline constexpr MessageInfo kChromeProcessDescriptor = {
    kChromeProcessDescriptorIndices, nullptr};

// Proto Message: ChromeThreadDescriptor
inline constexpr int kChromeThreadDescriptorIndices[] = {1, 2, -1};
inline constexpr MessageInfo kChromeThreadDescriptor = {
    kChromeThreadDescriptorIndices, nullptr};

// Proto Message: CounterDescriptor
inline constexpr int kCounterDescriptorIndices[] = {1, 3, 4, 5, -1};
inline constexpr MessageInfo kCounterDescriptor = {kCounterDescriptorIndices,
                                                   nullptr};

// Proto Message: StateDescriptor
inline constexpr int kStateDescriptorIndices[] = {-1};
inline constexpr MessageInfo kStateDescriptor = {kStateDescriptorIndices,
                                                 nullptr};

// Proto Message: TrackDescriptor
inline constexpr int kTrackDescriptorIndices[] = {1,  3,  4,  5,  6,  7,  8, 9,
                                                  10, 11, 12, 15, 17, 18, -1};
inline constexpr MessageInfo const* kTrackDescriptorComplexMessages[] = {
    nullptr,
    &kProcessDescriptor,
    &kThreadDescriptor,
    nullptr,
    &kChromeProcessDescriptor,
    &kChromeThreadDescriptor,
    &kCounterDescriptor,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &kStateDescriptor};
inline constexpr MessageInfo kTrackDescriptor = {
    kTrackDescriptorIndices, kTrackDescriptorComplexMessages};

// Proto Message: TraceUuid
inline constexpr int kTraceUuidIndices[] = {1, 2, -1};
inline constexpr MessageInfo kTraceUuid = {kTraceUuidIndices, nullptr};

// Proto Message: CSwitchEtwEvent
inline constexpr int kCSwitchEtwEventIndices[] = {1,  2,  3,  4,  5, 9,
                                                  10, 11, 12, 13, -1};
inline constexpr MessageInfo kCSwitchEtwEvent = {kCSwitchEtwEventIndices,
                                                 nullptr};

// Proto Message: ReadyThreadEtwEvent
inline constexpr int kReadyThreadEtwEventIndices[] = {1, 3, 5, 6, -1};
inline constexpr MessageInfo kReadyThreadEtwEvent = {
    kReadyThreadEtwEventIndices, nullptr};

// Proto Message: MemInfoEtwEvent
inline constexpr int kMemInfoEtwEventIndices[] = {1, 2, 3,  4,  5,  6,  7,
                                                  8, 9, 10, 11, 12, 13, -1};
inline constexpr MessageInfo kMemInfoEtwEvent = {kMemInfoEtwEventIndices,
                                                 nullptr};

// Proto Message: FileIoCreateEtwEvent
inline constexpr int kFileIoCreateEtwEventIndices[] = {1, 2, 3, 4, 5, 6, -1};
inline constexpr MessageInfo kFileIoCreateEtwEvent = {
    kFileIoCreateEtwEventIndices, nullptr};

// Proto Message: FileIoDirEnumEtwEvent
inline constexpr int kFileIoDirEnumEtwEventIndices[] = {1, 2, 3, 4, 5,
                                                        6, 7, 9, -1};
inline constexpr MessageInfo kFileIoDirEnumEtwEvent = {
    kFileIoDirEnumEtwEventIndices, nullptr};

// Proto Message: FileIoInfoEtwEvent
inline constexpr int kFileIoInfoEtwEventIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
inline constexpr MessageInfo kFileIoInfoEtwEvent = {kFileIoInfoEtwEventIndices,
                                                    nullptr};

// Proto Message: FileIoReadWriteEtwEvent
inline constexpr int kFileIoReadWriteEtwEventIndices[] = {1, 2, 3, 4, 5,
                                                          6, 7, 8, -1};
inline constexpr MessageInfo kFileIoReadWriteEtwEvent = {
    kFileIoReadWriteEtwEventIndices, nullptr};

// Proto Message: FileIoSimpleOpEtwEvent
inline constexpr int kFileIoSimpleOpEtwEventIndices[] = {1, 2, 3, 4, 5, -1};
inline constexpr MessageInfo kFileIoSimpleOpEtwEvent = {
    kFileIoSimpleOpEtwEventIndices, nullptr};

// Proto Message: FileIoOpEndEtwEvent
inline constexpr int kFileIoOpEndEtwEventIndices[] = {1, 2, 3, -1};
inline constexpr MessageInfo kFileIoOpEndEtwEvent = {
    kFileIoOpEndEtwEventIndices, nullptr};

// Proto Message: FileIoPathOperationEtwEvent
inline constexpr int kFileIoPathOperationEtwEventIndices[] = {1, 2, 3, 4,
                                                              5, 6, 8, -1};
inline constexpr MessageInfo kFileIoPathOperationEtwEvent = {
    kFileIoPathOperationEtwEventIndices, nullptr};

// Proto Message: EtwTraceEvent
inline constexpr int kEtwTraceEventIndices[] = {1, 2, 3,  4,  5,  6,  7,
                                                8, 9, 10, 11, 12, 14, -1};
inline constexpr MessageInfo const* kEtwTraceEventComplexMessages[] = {
    nullptr,
    &kCSwitchEtwEvent,
    &kReadyThreadEtwEvent,
    nullptr,
    nullptr,
    &kMemInfoEtwEvent,
    &kFileIoCreateEtwEvent,
    &kFileIoDirEnumEtwEvent,
    &kFileIoInfoEtwEvent,
    &kFileIoReadWriteEtwEvent,
    &kFileIoSimpleOpEtwEvent,
    &kFileIoOpEndEtwEvent,
    &kFileIoPathOperationEtwEvent};
inline constexpr MessageInfo kEtwTraceEvent = {kEtwTraceEventIndices,
                                               kEtwTraceEventComplexMessages};

// Proto Message: EtwTraceEventBundle
inline constexpr int kEtwTraceEventBundleIndices[] = {2, -1};
inline constexpr MessageInfo const* kEtwTraceEventBundleComplexMessages[] = {
    &kEtwTraceEvent};
inline constexpr MessageInfo kEtwTraceEventBundle = {
    kEtwTraceEventBundleIndices, kEtwTraceEventBundleComplexMessages};

// Proto Message: ChromeTrigger
inline constexpr int kChromeTriggerIndices[] = {2, 3, -1};
inline constexpr MessageInfo kChromeTrigger = {kChromeTriggerIndices, nullptr};

// Proto Message: TracePacket
inline constexpr int kTracePacketIndices[] = {6,  8,  10, 11, 12, 13,  35, 36,
                                              41, 42, 43, 44, 51, 54,  56, 58,
                                              59, 60, 87, 89, 95, 109, -1};
inline constexpr MessageInfo const* kTracePacketComplexMessages[] = {
    &kClockSnapshot,
    nullptr,
    nullptr,
    &kTrackEvent,
    &kInternedData,
    nullptr,
    &kTraceStats,
    nullptr,
    nullptr,
    nullptr,
    &kProcessDescriptor,
    &kThreadDescriptor,
    &kChromeMetadataPacket,
    &kStreamingProfilePacket,
    &kHeapGraph,
    nullptr,
    &kTracePacketDefaults,
    &kTrackDescriptor,
    nullptr,
    &kTraceUuid,
    &kEtwTraceEventBundle,
    &kChromeTrigger};
inline constexpr MessageInfo kTracePacket = {kTracePacketIndices,
                                             kTracePacketComplexMessages};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PRIVACY_FILTERED_FIELDS_INL_H_
