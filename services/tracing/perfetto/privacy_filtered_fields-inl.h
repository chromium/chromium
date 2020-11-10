// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_PRIVACY_FILTERED_FIELDS_INL_H_
#define SERVICES_TRACING_PERFETTO_PRIVACY_FILTERED_FIELDS_INL_H_

// This file is auto generated from internal copy of the TracePacket proto, that
// does not contain any privacy sensitive fields. Updates to this file should be
// made by changing internal copy and then running the generator script. Follow
// instructions at:
// https://goto.google.com/chrome-trace-privacy-filtered-fields

namespace tracing {

// A MessageInfo node created from a tree of TracePacket proto messages.
struct MessageInfo {
  // List of accepted field ids in the output for this message. The end of list
  // is marked by a -1.
  const int* accepted_field_ids;

  // List of sub messages that correspond to the accepted field ids list. There
  // is no end of list marker and the length is this list is equal to length of
  // |accepted_field_ids| - 1.
  const MessageInfo* const* const sub_messages;
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
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, -1};
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

// Proto Message: ComponentInfo
constexpr int kComponentInfoIndices[] = {1, 2, -1};
constexpr MessageInfo kComponentInfo = {kComponentInfoIndices, nullptr};

// Proto Message: ChromeLatencyInfo
constexpr int kChromeLatencyInfoIndices[] = {1, 2, 3, 4, 5, 6, -1};
constexpr MessageInfo const* kChromeLatencyInfoComplexMessages[] = {
    nullptr, nullptr, nullptr, &kComponentInfo, nullptr, nullptr};
constexpr MessageInfo kChromeLatencyInfo = {kChromeLatencyInfoIndices,
                                            kChromeLatencyInfoComplexMessages};

// Proto Message: ChromeFrameReporter
constexpr int kChromeFrameReporterIndices[] = {1, 2, 3, 4, -1};
constexpr MessageInfo kChromeFrameReporter = {kChromeFrameReporterIndices,
                                              nullptr};

// Proto Message: ChromeMessagePump
constexpr int kChromeMessagePumpIndices[] = {1, -1};
constexpr MessageInfo kChromeMessagePump = {kChromeMessagePumpIndices, nullptr};

// Proto Message: ChromeRendererSchedulerState
constexpr int kChromeRendererSchedulerStateIndices[] = {1, -1};
constexpr MessageInfo kChromeRendererSchedulerState = {
    kChromeRendererSchedulerStateIndices, nullptr};

// Proto Message: TrackEvent
constexpr int kTrackEventIndices[] = {1,  2,  3,  5,  6,  9,  10, 11, 12,
                                      16, 17, 24, 25, 26, 27, 28, 29, 30,
                                      31, 32, 33, 34, 35, 40, -1};
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
    &kChromeCompositorSchedulerState,
    &kChromeUserEvent,
    &kChromeKeyedService,
    &kChromeLegacyIpc,
    &kChromeHistogramSample,
    &kChromeLatencyInfo,
    nullptr,
    nullptr,
    &kChromeFrameReporter,
    &kSourceLocation,
    nullptr,
    &kChromeMessagePump,
    &kChromeRendererSchedulerState};
constexpr MessageInfo kTrackEvent = {kTrackEventIndices,
                                     kTrackEventComplexMessages};

// Proto Message: EventCategory
constexpr int kEventCategoryIndices[] = {1, 2, -1};
constexpr MessageInfo kEventCategory = {kEventCategoryIndices, nullptr};

// Proto Message: EventName
constexpr int kEventNameIndices[] = {1, 2, -1};
constexpr MessageInfo kEventName = {kEventNameIndices, nullptr};

// Proto Message: Frame
constexpr int kFrameIndices[] = {1, 3, 4, -1};
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

// Proto Message: InternedData
constexpr int kInternedDataIndices[] = {1, 2, 4, 6, 7, 16, 17, 19, -1};
constexpr MessageInfo const* kInternedDataComplexMessages[] = {
    &kEventCategory, &kEventName,       &kSourceLocation,      &kFrame,
    &kCallstack,     &kInternedBuildId, &kInternedMappingPath, &kMapping};
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
constexpr int kProcessDescriptorIndices[] = {1, 4, 5, -1};
constexpr MessageInfo kProcessDescriptor = {kProcessDescriptorIndices, nullptr};

// Proto Message: ThreadDescriptor
constexpr int kThreadDescriptorIndices[] = {1, 2, 4, 6, 7, -1};
constexpr MessageInfo kThreadDescriptor = {kThreadDescriptorIndices, nullptr};

// Proto Message: HistogramRule
constexpr int kHistogramRuleIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kHistogramRule = {kHistogramRuleIndices, nullptr};

// Proto Message: NamedRule
constexpr int kNamedRuleIndices[] = {1, -1};
constexpr MessageInfo kNamedRule = {kNamedRuleIndices, nullptr};

// Proto Message: TriggerRule
constexpr int kTriggerRuleIndices[] = {1, 2, 3, -1};
constexpr MessageInfo const* kTriggerRuleComplexMessages[] = {
    nullptr, &kHistogramRule, &kNamedRule};
constexpr MessageInfo kTriggerRule = {kTriggerRuleIndices,
                                      kTriggerRuleComplexMessages};

// Proto Message: TraceMetadata
constexpr int kTraceMetadataIndices[] = {1, 2, -1};
constexpr MessageInfo const* kTraceMetadataComplexMessages[] = {&kTriggerRule,
                                                                &kTriggerRule};
constexpr MessageInfo kTraceMetadata = {kTraceMetadataIndices,
                                        kTraceMetadataComplexMessages};

// Proto Message: ChromeMetadataPacket
constexpr int kChromeMetadataPacketIndices[] = {1, 2, -1};
constexpr MessageInfo const* kChromeMetadataPacketComplexMessages[] = {
    &kTraceMetadata, nullptr};
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
constexpr int kChromeProcessDescriptorIndices[] = {1, 2, 3, -1};
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
constexpr int kTrackDescriptorIndices[] = {1, 3, 4, 5, 6, 7, 8, -1};
constexpr MessageInfo const* kTrackDescriptorComplexMessages[] = {
    nullptr,
    &kProcessDescriptor,
    &kThreadDescriptor,
    nullptr,
    &kChromeProcessDescriptor,
    &kChromeThreadDescriptor,
    &kCounterDescriptor};
constexpr MessageInfo kTrackDescriptor = {kTrackDescriptorIndices,
                                          kTrackDescriptorComplexMessages};

// Proto Message: TracePacket
// EDIT: Manually allowlisted: 3 (trusted_uid).
constexpr int kTracePacketIndices[] = {3,  6,  8,  10, 11, 12, 13, 35, 36, 41,
                                       42, 43, 44, 51, 54, 56, 58, 59, 60, -1};
constexpr MessageInfo const* kTracePacketComplexMessages[] = {
    nullptr,
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
    &kTrackDescriptor};
constexpr MessageInfo kTracePacket = {kTracePacketIndices,
                                      kTracePacketComplexMessages};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PRIVACY_FILTERED_FIELDS_INL_H_
