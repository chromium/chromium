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
constexpr int kClockSnapshotIndices[] = {1, -1};
constexpr MessageInfo const* kClockSnapshotComplexMessages[] = {&kClock};
constexpr MessageInfo kClockSnapshot = {kClockSnapshotIndices,
                                        kClockSnapshotComplexMessages};

// Proto Message: TaskExecution
constexpr int kTaskExecutionIndices[] = {1, -1};
constexpr MessageInfo kTaskExecution = {kTaskExecutionIndices, nullptr};

// Proto Message: LegacyEvent
constexpr int kLegacyEventIndices[] = {1,  2,  3,  4,  6,  8,  9, 10,
                                       11, 12, 13, 14, 18, 19, -1};
constexpr MessageInfo kLegacyEvent = {kLegacyEventIndices, nullptr};

// Proto Message: TrackEvent
constexpr int kTrackEventIndices[] = {1, 2, 3, 5, 6, 9, 10, 11, 16, 17, -1};
constexpr MessageInfo const* kTrackEventComplexMessages[] = {
    nullptr, nullptr, nullptr, &kTaskExecution, &kLegacyEvent,
    nullptr, nullptr, nullptr, nullptr,         nullptr};
constexpr MessageInfo kTrackEvent = {kTrackEventIndices,
                                     kTrackEventComplexMessages};

// Proto Message: EventCategory
constexpr int kEventCategoryIndices[] = {1, 2, -1};
constexpr MessageInfo kEventCategory = {kEventCategoryIndices, nullptr};

// Proto Message: EventName
constexpr int kEventNameIndices[] = {1, 2, -1};
constexpr MessageInfo kEventName = {kEventNameIndices, nullptr};

// Proto Message: SourceLocation
constexpr int kSourceLocationIndices[] = {1, 2, 3, -1};
constexpr MessageInfo kSourceLocation = {kSourceLocationIndices, nullptr};

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
constexpr int kStreamingProfilePacketIndices[] = {1, 2, -1};
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
constexpr int kTrackEventDefaultsIndices[] = {10, -1};
constexpr MessageInfo kTrackEventDefaults = {kTrackEventDefaultsIndices,
                                             nullptr};

// Proto Message: TracePacketDefaults
constexpr int kTracePacketDefaultsIndices[] = {11, 58, -1};
constexpr MessageInfo const* kTracePacketDefaultsComplexMessages[] = {
    &kTrackEventDefaults, nullptr};
constexpr MessageInfo kTracePacketDefaults = {
    kTracePacketDefaultsIndices, kTracePacketDefaultsComplexMessages};

// Proto Message: TrackDescriptor
constexpr int kTrackDescriptorIndices[] = {1, 3, 4, -1};
constexpr MessageInfo const* kTrackDescriptorComplexMessages[] = {
    nullptr, &kProcessDescriptor, &kThreadDescriptor};
constexpr MessageInfo kTrackDescriptor = {kTrackDescriptorIndices,
                                          kTrackDescriptorComplexMessages};

// Proto Message: TracePacket
// EDIT: Manually whitelisted: 3 (trusted_uid).
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
