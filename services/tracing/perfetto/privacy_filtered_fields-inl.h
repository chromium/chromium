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
constexpr int kClockIndices[] = {1, 2, 3, 4, -1};
constexpr MessageInfo kClock = {kClockIndices, nullptr};

// Proto Message: ClockSnapshot
constexpr int kClockSnapshotIndices[] = {1, 2, -1};
constexpr MessageInfo const* kClockSnapshotComplexMessages[] = {&kClock,
                                                                nullptr};
constexpr MessageInfo kClockSnapshot = {kClockSnapshotIndices,
                                        kClockSnapshotComplexMessages};

// Proto Message: TaskExecution
constexpr int kTaskExecutionIndices[] = {1, -1};
constexpr MessageInfo kTaskExecution = {kTaskExecutionIndices, nullptr};

// Proto Message: LegacyEvent
constexpr int kLegacyEventIndices[] = {1,  2,  3,  4,  6,  8,  9, 10,
                                       11, 12, 13, 14, 18, 19, -1};
constexpr MessageInfo kLegacyEvent = {kLegacyEventIndices, nullptr};

// Proto Message: MajorState
constexpr int kMajorStateIndices[] = {1, 2, 3, 4, 5, -1};
constexpr MessageInfo kMajorState = {kMajorStateIndices, nullptr};

// Proto Message: MinorState
constexpr int kMinorStateIndices[] = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, -1};
constexpr MessageInfo kMinorState = {kMinorStateIndices, nullptr};

// Proto Message: ChromeCompositorStateMachine
constexpr int kChromeCompositorStateMachineIndices[] = {1, 2, -1};
constexpr MessageInfo const* kChromeCompositorStateMachineComplexMessages[] = {
    &kMajorState, &kMinorState};
constexpr MessageInfo kChromeCompositorStateMachine = {
    kChromeCompositorStateMachineIndices,
    kChromeCompositorStateMachineComplexMessages};

// Proto Message: SourceLocation
constexpr int kSourceLocationIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kSourceLocation = {kSourceLocationIndices, nullptr};

// Proto Message: BeginFrameArgs
constexpr int kBeginFrameArgsIndices[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1};
constexpr MessageInfo const* kBeginFrameArgsComplexMessages[] = {
    nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, &kSourceLocation};
constexpr MessageInfo kBeginFrameArgs = {kBeginFrameArgsIndices,
                                         kBeginFrameArgsComplexMessages};

// Proto Message: TimestampsInUs
constexpr int kTimestampsInUsIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
constexpr MessageInfo kTimestampsInUs = {kTimestampsInUsIndices, nullptr};

// Proto Message: BeginImplFrameArgs
constexpr int kBeginImplFrameArgsIndices[] = {1, 2, 3, 4, 5, 6, -1};
constexpr MessageInfo const* kBeginImplFrameArgsComplexMessages[] = {
    nullptr,          nullptr,          nullptr,
    &kBeginFrameArgs, &kBeginFrameArgs, &kTimestampsInUs};
constexpr MessageInfo kBeginImplFrameArgs = {
    kBeginImplFrameArgsIndices, kBeginImplFrameArgsComplexMessages};

// Proto Message: BeginFrameObserverState
constexpr int kBeginFrameObserverStateIndices[] = {1, 2, -1};
constexpr MessageInfo const* kBeginFrameObserverStateComplexMessages[] = {
    nullptr, &kBeginFrameArgs};
constexpr MessageInfo kBeginFrameObserverState = {
    kBeginFrameObserverStateIndices, kBeginFrameObserverStateComplexMessages};

// Proto Message: BeginFrameSourceState
constexpr int kBeginFrameSourceStateIndices[] = {1, 2, 3, 4, -1};
constexpr MessageInfo const* kBeginFrameSourceStateComplexMessages[] = {
    nullptr, nullptr, nullptr, &kBeginFrameArgs};
constexpr MessageInfo kBeginFrameSourceState = {
    kBeginFrameSourceStateIndices, kBeginFrameSourceStateComplexMessages};

// Proto Message: CompositorTimingHistory
constexpr int kCompositorTimingHistoryIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
constexpr MessageInfo kCompositorTimingHistory = {
    kCompositorTimingHistoryIndices, nullptr};

// Proto Message: ChromeCompositorSchedulerState
constexpr int kChromeCompositorSchedulerStateIndices[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, -1};
constexpr MessageInfo const* kChromeCompositorSchedulerStateComplexMessages[] =
    {&kChromeCompositorStateMachine,
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
constexpr MessageInfo kChromeCompositorSchedulerState = {
    kChromeCompositorSchedulerStateIndices,
    kChromeCompositorSchedulerStateComplexMessages};

// Proto Message: ChromeUserEvent
constexpr int kChromeUserEventIndices[] = {2, -1};
constexpr MessageInfo kChromeUserEvent = {kChromeUserEventIndices, nullptr};

// Proto Message: ChromeKeyedService
constexpr int kChromeKeyedServiceIndices[] = {1, -1};
constexpr MessageInfo kChromeKeyedService = {kChromeKeyedServiceIndices,
                                             nullptr};

// Proto Message: ChromeLegacyIpc
constexpr int kChromeLegacyIpcIndices[] = {1, 2, -1};
constexpr MessageInfo kChromeLegacyIpc = {kChromeLegacyIpcIndices, nullptr};

// Proto Message: ChromeHistogramSample
constexpr int kChromeHistogramSampleIndices[] = {1, 3, -1};
constexpr MessageInfo kChromeHistogramSample = {kChromeHistogramSampleIndices,
                                                nullptr};

// Proto Message: ChromeFrameReporter
constexpr int kChromeFrameReporterIndices[] = {1, 2, 3, 4,  5,  6,
                                               7, 8, 9, 10, 11, -1};
constexpr MessageInfo kChromeFrameReporter = {kChromeFrameReporterIndices,
                                              nullptr};

// Proto Message: ChromeMessagePump
constexpr int kChromeMessagePumpIndices[] = {1, 2, -1};
constexpr MessageInfo kChromeMessagePump = {kChromeMessagePumpIndices, nullptr};

// Proto Message: ChromeMojoEventInfo
constexpr int kChromeMojoEventInfoIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
constexpr MessageInfo kChromeMojoEventInfo = {kChromeMojoEventInfoIndices,
                                              nullptr};

// Proto Message: ChromeApplicationStateInfo
constexpr int kChromeApplicationStateInfoIndices[] = {1, -1};
constexpr MessageInfo kChromeApplicationStateInfo = {
    kChromeApplicationStateInfoIndices, nullptr};

// Proto Message: ChromeRendererSchedulerState
constexpr int kChromeRendererSchedulerStateIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kChromeRendererSchedulerState = {
    kChromeRendererSchedulerStateIndices, nullptr};

// Proto Message: ChromeWindowHandleEventInfo
constexpr int kChromeWindowHandleEventInfoIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kChromeWindowHandleEventInfo = {
    kChromeWindowHandleEventInfoIndices, nullptr};

// Proto Message: ChromeContentSettingsEventInfo
constexpr int kChromeContentSettingsEventInfoIndices[] = {1, -1};
constexpr MessageInfo kChromeContentSettingsEventInfo = {
    kChromeContentSettingsEventInfoIndices, nullptr};

// Proto Message: ChromeMemoryPressureNotification
constexpr int kChromeMemoryPressureNotificationIndices[] = {1, 2, -1};
constexpr MessageInfo kChromeMemoryPressureNotification = {
    kChromeMemoryPressureNotificationIndices, nullptr};

// Proto Message: ChromeTaskAnnotator
constexpr int kChromeTaskAnnotatorIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kChromeTaskAnnotator = {kChromeTaskAnnotatorIndices,
                                              nullptr};

// Proto Message: ChromeBrowserContext
constexpr int kChromeBrowserContextIndices[] = {1, 2, -1};
constexpr MessageInfo kChromeBrowserContext = {kChromeBrowserContextIndices,
                                               nullptr};

// Proto Message: ChromeProfileDestroyer
constexpr int kChromeProfileDestroyerIndices[] = {1, 2, 4, 5, 6, -1};
constexpr MessageInfo kChromeProfileDestroyer = {kChromeProfileDestroyerIndices,
                                                 nullptr};

// Proto Message: ChromeTaskPostedToDisabledQueue
constexpr int kChromeTaskPostedToDisabledQueueIndices[] = {2, 3, 4, -1};
constexpr MessageInfo kChromeTaskPostedToDisabledQueue = {
    kChromeTaskPostedToDisabledQueueIndices, nullptr};

// Proto Message: ChromeTaskGraphRunner
constexpr int kChromeTaskGraphRunnerIndices[] = {1, -1};
constexpr MessageInfo kChromeTaskGraphRunner = {kChromeTaskGraphRunnerIndices,
                                                nullptr};

// Proto Message: ChromeMessagePumpForUI
constexpr int kChromeMessagePumpForUIIndices[] = {1, 2, -1};
constexpr MessageInfo kChromeMessagePumpForUI = {kChromeMessagePumpForUIIndices,
                                                 nullptr};

// Proto Message: RenderFrameImplDeletion
constexpr int kRenderFrameImplDeletionIndices[] = {1, 2, 3, 4, -1};
constexpr MessageInfo kRenderFrameImplDeletion = {
    kRenderFrameImplDeletionIndices, nullptr};

// Proto Message: ShouldSwapBrowsingInstancesResult
constexpr int kShouldSwapBrowsingInstancesResultIndices[] = {1, 2, -1};
constexpr MessageInfo kShouldSwapBrowsingInstancesResult = {
    kShouldSwapBrowsingInstancesResultIndices, nullptr};

// Proto Message: FrameTreeNodeInfo
constexpr int kFrameTreeNodeInfoIndices[] = {1, 2, 3, 6, -1};
constexpr MessageInfo kFrameTreeNodeInfo = {kFrameTreeNodeInfoIndices, nullptr};

// Proto Message: ChromeHashedPerformanceMark
constexpr int kChromeHashedPerformanceMarkIndices[] = {1, 3, 5, 6, -1};
constexpr MessageInfo kChromeHashedPerformanceMark = {
    kChromeHashedPerformanceMarkIndices, nullptr};

// Proto Message: RenderProcessHost
constexpr int kRenderProcessHostIndices[] = {1, 3, 4, -1};
constexpr MessageInfo const* kRenderProcessHostComplexMessages[] = {
    nullptr, nullptr, &kChromeBrowserContext};
constexpr MessageInfo kRenderProcessHost = {kRenderProcessHostIndices,
                                            kRenderProcessHostComplexMessages};

// Proto Message: RenderProcessHostCleanup
constexpr int kRenderProcessHostCleanupIndices[] = {1, 2, 3, 4, 5, -1};
constexpr MessageInfo kRenderProcessHostCleanup = {
    kRenderProcessHostCleanupIndices, nullptr};

// Proto Message: RenderProcessHostListener
constexpr int kRenderProcessHostListenerIndices[] = {1, -1};
constexpr MessageInfo kRenderProcessHostListener = {
    kRenderProcessHostListenerIndices, nullptr};

// Proto Message: ChildProcessLauncherPriority
constexpr int kChildProcessLauncherPriorityIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kChildProcessLauncherPriority = {
    kChildProcessLauncherPriorityIndices, nullptr};

// Proto Message: ResourceBundle
constexpr int kResourceBundleIndices[] = {1, -1};
constexpr MessageInfo kResourceBundle = {kResourceBundleIndices, nullptr};

// Proto Message: ChromeWebAppBadNavigate
constexpr int kChromeWebAppBadNavigateIndices[] = {1, 2, 4, 5, 6, -1};
constexpr MessageInfo kChromeWebAppBadNavigate = {
    kChromeWebAppBadNavigateIndices, nullptr};

// Proto Message: ChromeExtensionId
constexpr int kChromeExtensionIdIndices[] = {2, -1};
constexpr MessageInfo kChromeExtensionId = {kChromeExtensionIdIndices, nullptr};

// Proto Message: SiteInstanceGroup
constexpr int kSiteInstanceGroupIndices[] = {1, 2, 3, -1};
constexpr MessageInfo const* kSiteInstanceGroupComplexMessages[] = {
    nullptr, nullptr, &kRenderProcessHost};
constexpr MessageInfo kSiteInstanceGroup = {kSiteInstanceGroupIndices,
                                            kSiteInstanceGroupComplexMessages};

// Proto Message: SiteInstance
constexpr int kSiteInstanceIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
constexpr MessageInfo const* kSiteInstanceComplexMessages[] = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &kSiteInstanceGroup};
constexpr MessageInfo kSiteInstance = {kSiteInstanceIndices,
                                       kSiteInstanceComplexMessages};

// Proto Message: RenderViewHost
constexpr int kRenderViewHostIndices[] = {1, 2, 3, 4, 5, -1};
constexpr MessageInfo kRenderViewHost = {kRenderViewHostIndices, nullptr};

// Proto Message: RenderFrameProxyHost
constexpr int kRenderFrameProxyHostIndices[] = {1, 2, 3, 4, 5, 6, -1};
constexpr MessageInfo kRenderFrameProxyHost = {kRenderFrameProxyHostIndices,
                                               nullptr};

// Proto Message: AndroidView
constexpr int kAndroidViewIndices[] = {1, 2, 3, 4, 5, 6, -1};
constexpr MessageInfo kAndroidView = {kAndroidViewIndices, nullptr};

// Proto Message: AndroidActivity
constexpr int kAndroidActivityIndices[] = {1, 2, -1};
constexpr MessageInfo const* kAndroidActivityComplexMessages[] = {
    nullptr, &kAndroidView};
constexpr MessageInfo kAndroidActivity = {kAndroidActivityIndices,
                                          kAndroidActivityComplexMessages};

// Proto Message: AndroidViewDump
constexpr int kAndroidViewDumpIndices[] = {1, -1};
constexpr MessageInfo const* kAndroidViewDumpComplexMessages[] = {
    &kAndroidActivity};
constexpr MessageInfo kAndroidViewDump = {kAndroidViewDumpIndices,
                                          kAndroidViewDumpComplexMessages};

// Proto Message: ParkableStringCompressInBackground
constexpr int kParkableStringCompressInBackgroundIndices[] = {1, -1};
constexpr MessageInfo kParkableStringCompressInBackground = {
    kParkableStringCompressInBackgroundIndices, nullptr};

// Proto Message: ParkableStringUnpark
constexpr int kParkableStringUnparkIndices[] = {1, 2, -1};
constexpr MessageInfo kParkableStringUnpark = {kParkableStringUnparkIndices,
                                               nullptr};

// Proto Message: ChromeSamplingProfilerSampleCollected
constexpr int kChromeSamplingProfilerSampleCollectedIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kChromeSamplingProfilerSampleCollected = {
    kChromeSamplingProfilerSampleCollectedIndices, nullptr};

// Proto Message: RenderFrameHost
constexpr int kRenderFrameHostIndices[] = {3, 6, 12, -1};
constexpr MessageInfo kRenderFrameHost = {kRenderFrameHostIndices, nullptr};

// Proto Message: RendererMainThreadTaskExecution
constexpr int kRendererMainThreadTaskExecutionIndices[] = {1, 2, 3, 4, 5, -1};
constexpr MessageInfo kRendererMainThreadTaskExecution = {
    kRendererMainThreadTaskExecutionIndices, nullptr};

// Proto Message: EventLatency
constexpr int kEventLatencyIndices[] = {1, 2, 4, 5, 6, -1};
constexpr MessageInfo kEventLatency = {kEventLatencyIndices, nullptr};

// Proto Message: ProcessSingleton
constexpr int kProcessSingletonIndices[] = {1, 2, -1};
constexpr MessageInfo kProcessSingleton = {kProcessSingletonIndices, nullptr};

// Proto Message: AndroidIPC
constexpr int kAndroidIPCIndices[] = {1, 2, -1};
constexpr MessageInfo kAndroidIPC = {kAndroidIPCIndices, nullptr};

// Proto Message: ChromeSqlDiagnostics
constexpr int kChromeSqlDiagnosticsIndices[] = {1, 2, 3, 4,  5, 6,
                                                7, 8, 9, 10, -1};
constexpr MessageInfo kChromeSqlDiagnostics = {kChromeSqlDiagnosticsIndices,
                                               nullptr};

// Proto Message: SequenceManagerTask
constexpr int kSequenceManagerTaskIndices[] = {1, 2, -1};
constexpr MessageInfo kSequenceManagerTask = {kSequenceManagerTaskIndices,
                                              nullptr};

// Proto Message: AndroidToolbar
constexpr int kAndroidToolbarIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kAndroidToolbar = {kAndroidToolbarIndices, nullptr};

// Proto Message: ActiveProcesses
constexpr int kActiveProcessesIndices[] = {1, -1};
constexpr MessageInfo kActiveProcesses = {kActiveProcessesIndices, nullptr};

// Proto Message: TabSwitchMeasurement
constexpr int kTabSwitchMeasurementIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kTabSwitchMeasurement = {kTabSwitchMeasurementIndices,
                                               nullptr};

// Proto Message: ScrollDeltas
constexpr int kScrollDeltasIndices[] = {1, 2,  3,  4,  5,  6,  7, 8,
                                        9, 10, 11, 12, 13, 14, -1};
constexpr MessageInfo kScrollDeltas = {kScrollDeltasIndices, nullptr};

// Proto Message: WinRenderAudioFromSource
constexpr int kWinRenderAudioFromSourceIndices[] = {1, 2, 3, 4,  5,  6,
                                                    7, 8, 9, 10, 11, -1};
constexpr MessageInfo kWinRenderAudioFromSource = {
    kWinRenderAudioFromSourceIndices, nullptr};

// Proto Message: MacAUHALStream
constexpr int kMacAUHALStreamIndices[] = {1, 2,  3,  4,  5,  6,  7, 8,
                                          9, 10, 11, 12, 13, 14, -1};
constexpr MessageInfo kMacAUHALStream = {kMacAUHALStreamIndices, nullptr};

// Proto Message: LinuxAlsaOutput
constexpr int kLinuxAlsaOutputIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
constexpr MessageInfo kLinuxAlsaOutput = {kLinuxAlsaOutputIndices, nullptr};

// Proto Message: LinuxPulseOutput
constexpr int kLinuxPulseOutputIndices[] = {1, 2, 3, 4, 5, -1};
constexpr MessageInfo kLinuxPulseOutput = {kLinuxPulseOutputIndices, nullptr};

// Proto Message: FrameSinkId
constexpr int kFrameSinkIdIndices[] = {1, 2, -1};
constexpr MessageInfo kFrameSinkId = {kFrameSinkIdIndices, nullptr};

// Proto Message: ChromeUnguessableToken
constexpr int kChromeUnguessableTokenIndices[] = {1, 2, -1};
constexpr MessageInfo kChromeUnguessableToken = {kChromeUnguessableTokenIndices,
                                                 nullptr};

// Proto Message: LocalSurfaceId
constexpr int kLocalSurfaceIdIndices[] = {1, 2, 3, -1};
constexpr MessageInfo const* kLocalSurfaceIdComplexMessages[] = {
    nullptr, nullptr, &kChromeUnguessableToken};
constexpr MessageInfo kLocalSurfaceId = {kLocalSurfaceIdIndices,
                                         kLocalSurfaceIdComplexMessages};

// Proto Message: ChromeGraphicsPipeline
constexpr int kChromeGraphicsPipelineIndices[] = {1, 2, 3, 4, 5, 6, 8, 9, -1};
constexpr MessageInfo const* kChromeGraphicsPipelineComplexMessages[] = {
    nullptr, &kFrameSinkId, nullptr, &kLocalSurfaceId,
    nullptr, nullptr,       nullptr, nullptr};
constexpr MessageInfo kChromeGraphicsPipeline = {
    kChromeGraphicsPipelineIndices, kChromeGraphicsPipelineComplexMessages};

// Proto Message: CrasUnified
constexpr int kCrasUnifiedIndices[] = {1, 2, 3, 4, 5, 6, 7, -1};
constexpr MessageInfo kCrasUnified = {kCrasUnifiedIndices, nullptr};

// Proto Message: LibunwindstackUnwinder
constexpr int kLibunwindstackUnwinderIndices[] = {1, 2, -1};
constexpr MessageInfo kLibunwindstackUnwinder = {kLibunwindstackUnwinderIndices,
                                                 nullptr};

// Proto Message: EventFrameValue
constexpr int kEventFrameValueIndices[] = {1, 2, -1};
constexpr MessageInfo kEventFrameValue = {kEventFrameValueIndices, nullptr};

// Proto Message: ScrollPredictorMetrics
constexpr int kScrollPredictorMetricsIndices[] = {1, 2, 3, 4, 5, 6, -1};
constexpr MessageInfo const* kScrollPredictorMetricsComplexMessages[] = {
    &kEventFrameValue, &kEventFrameValue, &kEventFrameValue,
    nullptr,           nullptr,           nullptr};
constexpr MessageInfo kScrollPredictorMetrics = {
    kScrollPredictorMetricsIndices, kScrollPredictorMetricsComplexMessages};

// Proto Message: PageLoad
constexpr int kPageLoadIndices[] = {1, -1};
constexpr MessageInfo kPageLoad = {kPageLoadIndices, nullptr};

// Proto Message: StartUp
constexpr int kStartUpIndices[] = {1, 3, -1};
constexpr MessageInfo kStartUp = {kStartUpIndices, nullptr};

// Proto Message: WebContentInteraction
constexpr int kWebContentInteractionIndices[] = {1, 2, -1};
constexpr MessageInfo kWebContentInteraction = {kWebContentInteractionIndices,
                                                nullptr};

// Proto Message: EventForwarder
constexpr int kEventForwarderIndices[] = {1, 2, 5, 6, 7, -1};
constexpr MessageInfo kEventForwarder = {kEventForwarderIndices, nullptr};

// Proto Message: TouchDispositionGestureFilter
constexpr int kTouchDispositionGestureFilterIndices[] = {1, -1};
constexpr MessageInfo kTouchDispositionGestureFilter = {
    kTouchDispositionGestureFilterIndices, nullptr};

// Proto Message: ViewClassName
constexpr int kViewClassNameIndices[] = {1, -1};
constexpr MessageInfo kViewClassName = {kViewClassNameIndices, nullptr};

// Proto Message: AnimationFrameTimingInfo
constexpr int kAnimationFrameTimingInfoIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kAnimationFrameTimingInfo = {
    kAnimationFrameTimingInfoIndices, nullptr};

// Proto Message: AnimationFrameScriptTimingInfo
constexpr int kAnimationFrameScriptTimingInfoIndices[] = {1, 2, 3, 9, 10, -1};
constexpr MessageInfo kAnimationFrameScriptTimingInfo = {
    kAnimationFrameScriptTimingInfoIndices, nullptr};

// Proto Message: ComponentInfo
constexpr int kComponentInfoIndices[] = {1, 2, -1};
constexpr MessageInfo kComponentInfo = {kComponentInfoIndices, nullptr};

// Proto Message: ChromeLatencyInfo2
constexpr int kChromeLatencyInfo2Indices[] = {1, 2, 3, 4,  5, 6,
                                              7, 8, 9, 10, -1};
constexpr MessageInfo const* kChromeLatencyInfo2ComplexMessages[] = {
    nullptr, nullptr, nullptr, &kComponentInfo, nullptr,
    nullptr, nullptr, nullptr, nullptr,         nullptr};
constexpr MessageInfo kChromeLatencyInfo2 = {
    kChromeLatencyInfo2Indices, kChromeLatencyInfo2ComplexMessages};

// Proto Message: EventTiming
constexpr int kEventTimingIndices[] = {1, 3, 4, 5, 6, 7, 8, 9, -1};
constexpr MessageInfo kEventTiming = {kEventTimingIndices, nullptr};

// Proto Message: TrackEvent
constexpr int kTrackEventIndices[] = {
    1,    2,    3,    5,    6,    9,    10,   11,   12,   16,   17,   22,
    23,   24,   25,   26,   27,   28,   30,   31,   32,   33,   34,   35,
    36,   38,   39,   40,   41,   42,   43,   47,   48,   1001, 1002, 1003,
    1004, 1005, 1006, 1007, 1008, 1009, 1010, 1011, 1012, 1013, 1014, 1015,
    1016, 1017, 1018, 1019, 1020, 1021, 1022, 1023, 1024, 1025, 1028, 1031,
    1032, 1033, 1034, 1036, 1038, 1039, 1040, 1041, 1042, 1046, 1047, 1048,
    1049, 1050, 1051, 1052, 1053, 1054, 1055, 1056, 1057, 1058, 1059, 1060,
    1061, 1064, 1065, 1068, 1069, -1};
constexpr MessageInfo const* kTrackEventComplexMessages[] = {
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
    &kChromeLatencyInfo2,
    &kEventTiming};
constexpr MessageInfo kTrackEvent = {kTrackEventIndices,
                                     kTrackEventComplexMessages};

// Proto Message: EventCategory
constexpr int kEventCategoryIndices[] = {1, 2, -1};
constexpr MessageInfo kEventCategory = {kEventCategoryIndices, nullptr};

// Proto Message: EventName
constexpr int kEventNameIndices[] = {1, 2, -1};
constexpr MessageInfo kEventName = {kEventNameIndices, nullptr};

// Proto Message: InternedString
constexpr int kInternedStringIndices[] = {1, 2, -1};
constexpr MessageInfo kInternedString = {kInternedStringIndices, nullptr};

// Proto Message: Frame
constexpr int kFrameIndices[] = {1, 2, 3, 4, -1};
constexpr MessageInfo kFrame = {kFrameIndices, nullptr};

// Proto Message: Callstack
constexpr int kCallstackIndices[] = {1, 2, -1};
constexpr MessageInfo kCallstack = {kCallstackIndices, nullptr};

// Proto Message: InternedBuildId
constexpr int kInternedBuildIdIndices[] = {1, 2, -1};
constexpr MessageInfo kInternedBuildId = {kInternedBuildIdIndices, nullptr};

// Proto Message: InternedMappingPath
constexpr int kInternedMappingPathIndices[] = {1, 2, -1};
constexpr MessageInfo kInternedMappingPath = {kInternedMappingPathIndices,
                                              nullptr};

// Proto Message: Mapping
constexpr int kMappingIndices[] = {1, 2, 3, 4, 5, 7, -1};
constexpr MessageInfo kMapping = {kMappingIndices, nullptr};

// Proto Message: UnsymbolizedSourceLocation
constexpr int kUnsymbolizedSourceLocationIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kUnsymbolizedSourceLocation = {
    kUnsymbolizedSourceLocationIndices, nullptr};

// Proto Message: InternedData
constexpr int kInternedDataIndices[] = {1, 2, 4, 5, 6, 7, 16, 17, 19, 28, -1};
constexpr MessageInfo const* kInternedDataComplexMessages[] = {
    &kEventCategory,   &kEventName,
    &kSourceLocation,  &kInternedString,
    &kFrame,           &kCallstack,
    &kInternedBuildId, &kInternedMappingPath,
    &kMapping,         &kUnsymbolizedSourceLocation};
constexpr MessageInfo kInternedData = {kInternedDataIndices,
                                       kInternedDataComplexMessages};

// Proto Message: BufferStats
constexpr int kBufferStatsIndices[] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                       11, 12, 13, 14, 15, 16, 17, 18, 19, -1};
constexpr MessageInfo kBufferStats = {kBufferStatsIndices, nullptr};

// Proto Message: TraceStats
constexpr int kTraceStatsIndices[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1};
constexpr MessageInfo const* kTraceStatsComplexMessages[] = {
    &kBufferStats, nullptr, nullptr, nullptr, nullptr,
    nullptr,       nullptr, nullptr, nullptr, nullptr};
constexpr MessageInfo kTraceStats = {kTraceStatsIndices,
                                     kTraceStatsComplexMessages};

// Proto Message: ProcessDescriptor
constexpr int kProcessDescriptorIndices[] = {1, 4, 5, 7, -1};
constexpr MessageInfo kProcessDescriptor = {kProcessDescriptorIndices, nullptr};

// Proto Message: ThreadDescriptor
constexpr int kThreadDescriptorIndices[] = {1, 2, 4, 6, 7, -1};
constexpr MessageInfo kThreadDescriptor = {kThreadDescriptorIndices, nullptr};

// Proto Message: HistogramRule
constexpr int kHistogramRuleIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kHistogramRule = {kHistogramRuleIndices, nullptr};

// Proto Message: NamedRule
constexpr int kNamedRuleIndices[] = {1, 2, -1};
constexpr MessageInfo kNamedRule = {kNamedRuleIndices, nullptr};

// Proto Message: TriggerRule
constexpr int kTriggerRuleIndices[] = {1, 2, 3, 4, -1};
constexpr MessageInfo const* kTriggerRuleComplexMessages[] = {
    nullptr, &kHistogramRule, &kNamedRule, nullptr};
constexpr MessageInfo kTriggerRule = {kTriggerRuleIndices,
                                      kTriggerRuleComplexMessages};

// Proto Message: BackgroundTracingMetadata
constexpr int kBackgroundTracingMetadataIndices[] = {1, 2, 3, -1};
constexpr MessageInfo const* kBackgroundTracingMetadataComplexMessages[] = {
    &kTriggerRule, &kTriggerRule, nullptr};
constexpr MessageInfo kBackgroundTracingMetadata = {
    kBackgroundTracingMetadataIndices,
    kBackgroundTracingMetadataComplexMessages};

// Proto Message: FinchHash
constexpr int kFinchHashIndices[] = {1, 2, -1};
constexpr MessageInfo kFinchHash = {kFinchHashIndices, nullptr};

// Proto Message: ChromeMetadataPacket
constexpr int kChromeMetadataPacketIndices[] = {1, 2, 3, 4, -1};
constexpr MessageInfo const* kChromeMetadataPacketComplexMessages[] = {
    &kBackgroundTracingMetadata, nullptr, nullptr, &kFinchHash};
constexpr MessageInfo kChromeMetadataPacket = {
    kChromeMetadataPacketIndices, kChromeMetadataPacketComplexMessages};

// Proto Message: StreamingProfilePacket
constexpr int kStreamingProfilePacketIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kStreamingProfilePacket = {kStreamingProfilePacketIndices,
                                                 nullptr};

// Proto Message: HeapGraphObject
constexpr int kHeapGraphObjectIndices[] = {1, 2, 3, 4, 5, -1};
constexpr MessageInfo kHeapGraphObject = {kHeapGraphObjectIndices, nullptr};

// Proto Message: InternedHeapGraphObjectTypes
constexpr int kInternedHeapGraphObjectTypesIndices[] = {1, 2, -1};
constexpr MessageInfo kInternedHeapGraphObjectTypes = {
    kInternedHeapGraphObjectTypesIndices, nullptr};

// Proto Message: InternedHeapGraphReferenceFieldNames
constexpr int kInternedHeapGraphReferenceFieldNamesIndices[] = {1, 2, -1};
constexpr MessageInfo kInternedHeapGraphReferenceFieldNames = {
    kInternedHeapGraphReferenceFieldNamesIndices, nullptr};

// Proto Message: HeapGraph
constexpr int kHeapGraphIndices[] = {1, 2, 3, 4, 5, 6, -1};
constexpr MessageInfo const* kHeapGraphComplexMessages[] = {
    nullptr,
    &kHeapGraphObject,
    &kInternedHeapGraphObjectTypes,
    &kInternedHeapGraphReferenceFieldNames,
    nullptr,
    nullptr};
constexpr MessageInfo kHeapGraph = {kHeapGraphIndices,
                                    kHeapGraphComplexMessages};

// Proto Message: TrackEventDefaults
constexpr int kTrackEventDefaultsIndices[] = {11, 31, -1};
constexpr MessageInfo kTrackEventDefaults = {kTrackEventDefaultsIndices,
                                             nullptr};

// Proto Message: TracePacketDefaults
constexpr int kTracePacketDefaultsIndices[] = {11, 58, -1};
constexpr MessageInfo const* kTracePacketDefaultsComplexMessages[] = {
    &kTrackEventDefaults, nullptr};
constexpr MessageInfo kTracePacketDefaults = {
    kTracePacketDefaultsIndices, kTracePacketDefaultsComplexMessages};

// Proto Message: ChromeProcessDescriptor
constexpr int kChromeProcessDescriptorIndices[] = {1, 2, 3, 5, -1};
constexpr MessageInfo kChromeProcessDescriptor = {
    kChromeProcessDescriptorIndices, nullptr};

// Proto Message: ChromeThreadDescriptor
constexpr int kChromeThreadDescriptorIndices[] = {1, 2, -1};
constexpr MessageInfo kChromeThreadDescriptor = {kChromeThreadDescriptorIndices,
                                                 nullptr};

// Proto Message: CounterDescriptor
constexpr int kCounterDescriptorIndices[] = {1, 3, 4, 5, -1};
constexpr MessageInfo kCounterDescriptor = {kCounterDescriptorIndices, nullptr};

// Proto Message: TrackDescriptor
constexpr int kTrackDescriptorIndices[] = {1, 3, 4, 5, 6, 7, 8, 9, 10, -1};
constexpr MessageInfo const* kTrackDescriptorComplexMessages[] = {
    nullptr,
    &kProcessDescriptor,
    &kThreadDescriptor,
    nullptr,
    &kChromeProcessDescriptor,
    &kChromeThreadDescriptor,
    &kCounterDescriptor,
    nullptr,
    nullptr};
constexpr MessageInfo kTrackDescriptor = {kTrackDescriptorIndices,
                                          kTrackDescriptorComplexMessages};

// Proto Message: TraceUuid
constexpr int kTraceUuidIndices[] = {1, 2, -1};
constexpr MessageInfo kTraceUuid = {kTraceUuidIndices, nullptr};

// Proto Message: ChromeTrigger
constexpr int kChromeTriggerIndices[] = {2, -1};
constexpr MessageInfo kChromeTrigger = {kChromeTriggerIndices, nullptr};

// Proto Message: TracePacket
constexpr int kTracePacketIndices[] = {6,  8,  10, 11, 12,  13, 35, 36,
                                       41, 42, 43, 44, 51,  54, 56, 58,
                                       59, 60, 87, 89, 109, -1};
constexpr MessageInfo const* kTracePacketComplexMessages[] = {
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
    &kChromeTrigger};
constexpr MessageInfo kTracePacket = {kTracePacketIndices,
                                      kTracePacketComplexMessages};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PRIVACY_FILTERED_FIELDS_INL_H_
