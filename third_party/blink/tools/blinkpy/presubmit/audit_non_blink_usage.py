#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for checking for disallowed usage of non-Blink declarations.

The scanner assumes that usage of non-Blink code is always namespace qualified.
Identifiers in the global namespace are always ignored. For convenience, the
script can be run in standalone mode to check for existing violations.

Example command:

$ git ls-files third_party/blink \
    | python3 third_party/blink/tools/blinkpy/presubmit/audit_non_blink_usage.py
"""

from __future__ import print_function

import os
import re
import sys

_DISALLOW_NON_BLINK_MOJOM = (
    # network::mojom::Foo is allowed to use as non-blink mojom type.
    # mojom::RendererContentSettingsPtr is allowed.
    '(?!network::)(\w+::)?mojom::(?!RendererContentSettingsPtr)(?!blink).+',
    'Using non-blink mojom types, consider using "::mojom::blink::Foo" instead '
    'of "::mojom::Foo" unless you have clear reasons not to do so.',
    'Warning')

_DISALLOW_CONTINUATION_DATA_ = (
    '.*(Get|Set)ContinuationPreservedEmbedderData.*',
    '[Get|Set]ContinuationPreservedEmbedderData does not support multiple '
    'clients.')

_CONFIG = [
    {
        'paths': ['third_party/blink/'],
        'allowed': [
            # absl
            'absl::Cleanup',
            'absl::get',
            'absl::get_if',
            'absl::holds_alternative',
            'absl::in_place',
            'absl::in_place_type',
            'absl::int128',
            'absl::Int128High64',
            'absl::Int128Low64',
            'absl::MakeInt128',
            'absl::MakeUint128',
            'absl::monostate',
            'absl::uint128',
            'absl::Uint128High64',
            'absl::Uint128Low64',
            'absl::variant',
            'absl::visit',

            # //base constructs that are allowed everywhere
            'base::(byte_)?span_from_ref',
            'base::AdoptRef',
            'base::ApplyMetadataToPastSamples',
            'base::as_byte_span',
            'base::as_byte_span',
            'base::as_bytes',
            'base::as_bytes',
            'base::as_chars',
            'base::as_chars',
            'base::as_string_view',
            'base::as_string_view',
            'base::as_writable_byte_span',
            'base::as_writable_bytes',
            'base::as_writable_bytes',
            'base::as_writable_chars',
            'base::AutoReset',
            'base::bit_cast',
            'base::CheckedContiguousIterator',
            'base::ConditionVariable',
            'base::Contains',
            'base::CPU',
            'base::Days',
            'base::DefaultTickClock',
            'base::ElapsedTimer',
            'base::EnumSet',
            'base::expected',
            'base::File',
            'base::FileErrorOr',
            'base::FilePath',
            'base::FunctionRef',
            'base::GetUniqueIdForProcess',
            'base::HashingLRUCache',
            'base::HashInts',
            'base::HeapArray',
            'base::Hertz',
            'base::HexStringToUInt64',
            'base::Hours',
            'base::i18n::TextDirection',
            'base::i18n::ToChar16Ptr',
            'base::i18n::ToUCharPtr',
            'base::JobDelegate',
            'base::JobHandle',
            'base::Location',
            'base::make_span',
            'base::MakeRefCounted',
            'base::MatcherStringPattern',
            'base::MatchPattern',
            'base::MessagePump',
            'base::MetricsSubSampler',
            'base::Microseconds',
            'base::Milliseconds',
            'base::Minutes',
            'base::Nanoseconds',
            'base::NotFatalUntil',
            'base::optional_ref',
            'base::OptionalFromPtr',
            'base::OptionalToPtr',
            'base::Overloaded',
            'base::PassKey',
            'base::PersistentHash',
            'base::PlatformThread',
            'base::PlatformThreadId',
            'base::PostJob',
            'base::PowerMonitor',
            'base::Process',
            'base::RadToDeg',
            'base::ranges::.+',
            'base::raw_span',
            'base::RefCountedData',
            'base::RemoveChars',
            'base::RepeatingTimer',
            'base::RunLoop',
            'base::SampleMetadataScope',
            'base::ScopedAllowBlocking',
            'base::ScopedClosureRunner',
            'base::ScopedFD',
            'base::Seconds',
            'base::sequence_manager::TaskTimeObserver',
            'base::SequencedTaskRunner',
            'base::SingleThreadTaskRunner',
            'base::span',
            'base::span(_with_nul)?_from_cstring',
            'base::Span(OrSize|Reader|Writer)',
            'base::StringPiece',
            'base::SubstringSetMatcher',
            'base::SysInfo',
            'base::ThreadChecker',
            'base::ThreadTicks',
            'base::ThreadType',
            'base::TickClock',
            'base::Time',
            'base::TimeDelta',
            'base::TimeTicks',
            'base::to_underlying',
            'base::Token',
            'base::trace_event::.*',
            'base::unexpected',
            'base::UnguessableToken',
            'base::UnguessableTokenHash',
            'base::UnlocalizedTimeFormatWithPattern',
            'base::Uuid',
            'base::ValuesEquivalent',
            'base::WeakPtr',
            'base::WeakPtrFactory',
            'base::WrapRefCounted',
            'logging::GetVlogLevel',
            'logging::SetLogItems',

            # //base/task/bind_post_task.h
            'base::BindPostTask',

            # //base/types/expected.h
            'base::expected',
            'base::ok',
            'base::unexpected',

            # //base/functional/bind.h
            'base::IgnoreResult',

            # //base/bits.h
            'base::bits::.+',

            # //base/observer_list.h.
            'base::CheckedObserver',
            'base::ObserverList',

            # //base/functional/callback_helpers.h.
            'base::DoNothing',
            'base::IgnoreArgs',
            'base::SplitOnceCallback',

            # //base/functional/callback.h is allowed, but you need to use
            # WTF::Bind or WTF::BindRepeating to create callbacks in
            # //third_party/blink/renderer.
            'base::BarrierCallback',
            'base::BarrierClosure',
            'base::NullCallback',
            'base::OnceCallback',
            'base::OnceClosure',
            'base::RepeatingCallback',
            'base::RepeatingClosure',

            # //base/cancelable_callback.h
            'base::CancelableOnceCallback',
            'base::CancelableOnceClosure',
            'base::CancelableRepeatingCallback',
            'base::CancelableRepeatingClosure',

            # //base/memory/ptr_util.h.
            'base::WrapUnique',

            # //base/containers/adapters.h
            'base::Reversed',

            # //base/metrics/histogram_functions.h
            'base::UmaHistogram.+',

            # //base/metrics/histogram.h
            'base::Histogram',
            'base::HistogramBase',
            'base::LinearHistogram',

            # //base/metrics/field_trial_params.h.
            'base::GetFieldTrialParamByFeatureAsBool',
            'base::GetFieldTrialParamByFeatureAsDouble',
            'base::GetFieldTrialParamByFeatureAsInt',
            'base::GetFieldTrialParamValueByFeature',

            # //base/numerics/safe_conversions.h.
            'base::as_signed',
            'base::as_unsigned',
            'base::checked_cast',
            'base::ClampCeil',
            'base::ClampFloor',
            'base::ClampRound',
            'base::IsTypeInRangeForNumericType',
            'base::IsValueInRangeForNumericType',
            'base::IsValueNegative',
            'base::MakeStrictNum',
            'base::SafeUnsignedAbs',
            'base::saturated_cast',
            'base::strict_cast',
            'base::StrictNumeric',

            # //base/synchronization/lock.h.
            'base::AutoLock',
            'base::AutoTryLock',
            'base::AutoUnlock',
            'base::Lock',

            # //base/synchronization/waitable_event.h.
            'base::WaitableEvent',

            # //base/numerics/checked_math.h.
            'base::CheckAdd',
            'base::CheckAnd',
            'base::CheckDiv',
            'base::CheckedNumeric',
            'base::CheckLsh',
            'base::CheckMax',
            'base::CheckMin',
            'base::CheckMod',
            'base::CheckMul',
            'base::CheckOr',
            'base::CheckRsh',
            'base::CheckSub',
            'base::CheckXor',
            'base::IsValidForType',
            'base::MakeCheckedNum',
            'base::ValueOrDefaultForType',
            'base::ValueOrDieForType',

            # //base/numerics/clamped_math.h.
            'base::ClampAdd',
            'base::ClampedNumeric',
            'base::ClampMax',
            'base::ClampMin',
            'base::ClampSub',
            'base::MakeClampedNum',

            # //base/strings/strcat.h.
            'base::StrAppend',
            'base::StrCat',

            # Debugging helpers from //base/debug are allowed everywhere.
            'base::debug::.+',

            # Base atomic utilities
            'base::AtomicFlag',
            'base::AtomicSequenceNumber',

            # Task traits
            'base::MayBlock',
            'base::SingleThreadTaskRunnerThreadMode',
            'base::TaskPriority',
            'base::TaskShutdownBehavior',
            'base::TaskTraits',
            'base::ThreadPolicy',
            'base::ThreadPool',
            'base::WithBaseSyncPrimitives',

            # Byte order
            'base::(numerics::)?((I|U)(8|16|32|64)|(Float|Double))(To|From)(Big|Little|Native)Endian',
            'base::(numerics::)?ByteSwap',
            'base::BigEndian(Reader|Writer)',

            # (Cryptographic) random number generation
            'base::RandBytes',
            'base::RandBytesAsString',
            'base::RandDouble',
            'base::RandGenerator',
            'base::RandInt',
            'base::RandUint64',

            # Feature list checking.
            "base::GetFieldTrial.*",
            'base::Feature.*',
            'base::FEATURE_.+',
            'base::features::.+',
            'features::.+',

            # Time
            'base::Clock',
            'base::DefaultClock',
            'base::DefaultTickClock',
            'base::TestMockTimeTaskRunner',
            'base::TickClock',

            # State transition checking
            'base::StateTransitions',

            # Shared memory
            'base::MappedReadOnlyRegion',
            'base::ReadOnlySharedMemoryMapping',
            'base::ReadOnlySharedMemoryRegion',
            'base::StructuredSharedMemory',
            'base::UnsafeSharedMemoryRegion',
            'base::WritableSharedMemoryMapping',
            'base::subtle::SharedAtomic',
        ]
    },
    {
        'paths': ['third_party/blink/common/'],
        'allowed': [
            # By definition, files in common are expected to use STL types.
            'std::.+',

            # Blink code shouldn't need to be qualified with the Blink namespace,
            # but there are exceptions, e.g. traits for Mojo.
            'blink::.+',
        ],
    },
    {
        'paths': ['third_party/blink/common/indexeddb/indexeddb_key.cc'],
        'allowed': ['base::HexEncode'],
    },
    {
        'paths': [
            'third_party/blink/common/interest_group/interest_group.cc',
            'third_party/blink/public/common/interest_group/interest_group.h'
        ],
        'allowed': [
            # For hashing of k-anonymity keys
            'crypto::SHA256HashString',

            # Types used to compute k-anonymity keys.
            "url::Origin",
            "GURL",
        ],
    },
    {
        'paths': ['third_party/blink/common/mime_util/'],
        'allowed': [
            # sets of ASCII strings are used
            'base::StartsWith',
            'base::ToLowerASCII',
            'base::MakeFixedFlatSet',

            # delegating to MIME utilities in other components
            'net::MatchesMimeType',
            'media::IsSupportedMediaMimeType',
        ],
    },
    {
        'paths': [
            'third_party/blink/common/page_state/page_state_serialization.cc',
        ],
        'allowed': [
            'base::ToVector',
            'mojom::Element',
            'network::DataElementBytes',
        ],
    },
    {
        'paths': [
            'third_party/blink/common/performance/performance_scenarios.cc',
        ],
        'allowed': [
            # Used in both browser and renderer process so can't use Oilpan.
            'base::NoDestructor',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/'],
        'allowed': [
            # TODO(dcheng): Should these be in a more specific config?
            'gfx::ColorSpace',
            'gfx::CubicBezier',
            'gfx::HDRMetadata',
            'gfx::HdrMetadataExtendedRange',
            'gfx::ICCProfile',

            # For fast cos/sin functions
            'gfx::SinCosDegrees',

            # //base/allocator/partition_allocator/src/partition_alloc/partition_alloc_constants.h
            'partition_alloc::internal::kAlignment',

            # PartitionAlloc
            'base::PartitionAllocReturnNull',
            'base::PartitionAllocZeroFill',
            'base::PartitionFree',

            # For TaskObserver.
            'base::PendingTask',

            # cc painting and raster types.
            'cc::AuxImage',
            'cc::CategorizedWorkerPool',
            'cc::ColorFilter',
            'cc::DrawLooper',
            'cc::InspectablePaintRecorder',
            'cc::InspectableRecordPaintCanvas',
            'cc::NodeId',
            'cc::NodeInfo',
            'cc::PaintCanvas',
            'cc::PaintFlags',
            'cc::PaintImage',
            'cc::PaintImageBuilder',
            'cc::PaintRecord',
            'cc::PaintShader',
            'cc::PaintWorkletInput',
            'cc::PathEffect',
            'cc::RecordPaintCanvas',
            'cc::RefCountedBuffer',
            'cc::UsePaintCache',

            # Chromium geometry types.
            'gfx::DecomposedTransform',
            'gfx::Insets',
            'gfx::InsetsF',
            'gfx::Outsets',
            'gfx::OutsetsF',
            'gfx::Point',
            'gfx::Point3F',
            'gfx::PointF',
            'gfx::QuadF',
            'gfx::Quaternion',
            'gfx::Rect',
            'gfx::RectF',
            'gfx::RRectF',
            'gfx::Size',
            'gfx::SizeF',
            'gfx::Transform',
            'gfx::Vector2d',
            'gfx::Vector2dF',
            'gfx::Vector3dF',

            # Chromium geometry operations.
            'cc::MathUtil',
            'gfx::AngleBetweenVectorsInDegrees',
            'gfx::BoundingRect',
            'gfx::ComputeApproximateMaxScale',
            'gfx::CrossProduct',
            'gfx::DotProduct',
            'gfx::IntersectRects',
            'gfx::MapRect',
            'gfx::MapRect',
            'gfx::MaximumCoveredRect',
            'gfx::PointAtOffsetFromOrigin',
            'gfx::PointFToSkPoint',
            'gfx::PointToSkIPoint',
            'gfx::RectFToSkRect',
            'gfx::RectToSkIRect',
            'gfx::RectToSkRect',
            'gfx::ScaleInsets',
            'gfx::ScalePoint',
            'gfx::ScaleRect',
            'gfx::ScaleSize',
            'gfx::ScaleToCeiledSize',
            'gfx::ScaleToEnclosedRect',
            'gfx::ScaleToEnclosingRect',
            'gfx::ScaleToFlooredSize',
            'gfx::ScaleToRoundedPoint',
            'gfx::ScaleToRoundedRect',
            'gfx::ScaleToRoundedSize',
            'gfx::ScaleVector2d',
            'gfx::ScaleVector3d',
            'gfx::SizeFToSkSize',
            'gfx::SizeToSkISize',
            'gfx::SkIPointToPoint',
            'gfx::SkIRectToRect',
            'gfx::SkISizeToSize',
            'gfx::SkPointToPointF',
            'gfx::SkRectToRectF',
            'gfx::SkSizeToSizeF',
            'gfx::SubtractRects',
            'gfx::ToCeiledPoint',
            'gfx::ToCeiledSize',
            'gfx::ToCeiledVector2d',
            'gfx::ToEnclosedRect',
            'gfx::ToEnclosingRect',
            'gfx::ToFlooredPoint',
            'gfx::ToFlooredSize',
            'gfx::ToFlooredVector2d',
            'gfx::ToRoundedPoint',
            'gfx::ToRoundedRect',
            'gfx::ToRoundedSize',
            'gfx::ToRoundedVector2d',
            'gfx::TransposePoint',
            'gfx::TransposeRect',
            'gfx::TransposeSize',
            'gfx::TryComputeTransform2dScaleComponents',
            'gfx::UnionRects',

            # Range type.
            'gfx::Range',

            # Mac CALayer result (error code)
            'gfx::CALayerResult',
            'gfx::kCALayerUnknownDidNotSwap',
            'gfx::kCALayerUnknownNoWidget',

            # Wrapper of SkRegion used in Chromium.
            'cc::Region',

            # A geometric set of TouchActions associated with areas, and only
            # depends on the geometry types above.
            'cc::TouchActionRegion',

            # Selection bounds.
            'cc::LayerSelection',
            'cc::LayerSelectionBound',
            'gfx::SelectionBound',

            # cc::Layers.
            'cc::Layer',
            'cc::LayerClient',
            'cc::LayerTreeDebugState',
            'cc::LayerTreeHost',
            'cc::PictureLayer',
            'cc::SurfaceLayer',

            # cc::Layer helper data structs.
            'cc::AnchorPositionScrollData',
            'cc::BrowserControlsParams',
            'cc::ElementId',
            'cc::LayerPositionConstraint',
            'cc::OverscrollBehavior',
            'cc::Scrollbar',
            'cc::ScrollbarLayerBase',
            'cc::ScrollbarOrientation',
            'cc::ScrollbarPart',
            'cc::StickyPositionConstraint',
            'cc::StickyPositionNodeData',
            'cc::ViewportLayers',

            # cc::Layer helper enums.
            'cc::BrowserControlsState',
            'cc::EventListenerClass',
            'cc::EventListenerProperties',
            'cc::HitTestOpaqueness',
            'cc::HORIZONTAL',
            'cc::THUMB',
            'cc::TRACK_BUTTONS_TICKMARKS',
            'cc::VERTICAL',

            # Animation
            "cc::AnimationHost",
            "cc::AnimationIdProvider",
            "cc::AnimationTimeline",
            "cc::FilterKeyframe",
            "cc::KeyframedFilterAnimationCurve",
            "cc::KeyframeModel",
            "cc::ScrollOffsetAnimationCurveFactory",
            "cc::TargetProperty",
            "gfx::AnimationCurve",
            "gfx::ColorKeyframe",
            "gfx::FloatKeyframe",
            "gfx::KeyframedColorAnimationCurve",
            "gfx::KeyframedFloatAnimationCurve",
            "gfx::KeyframedTransformAnimationCurve",
            "gfx::LinearEasingPoint",
            "gfx::TimingFunction",
            "gfx::TransformKeyframe",
            "gfx::TransformOperations",

            # UMA Enums
            'cc::PaintHoldingCommitTrigger',
            'cc::PaintHoldingReason',

            # Scrolling
            'cc::BrowserControlsOffsetTagsInfo',
            'cc::kManipulationInfoNone',
            'cc::kManipulationInfoPinchZoom',
            'cc::kManipulationInfoPrecisionTouchPad',
            'cc::kManipulationInfoScrollbar',
            'cc::kManipulationInfoTouch',
            'cc::kManipulationInfoWheel',
            'cc::kMinFractionToStepWhenPaging',
            'cc::kPercentDeltaForDirectionalScroll',
            'cc::kPixelsPerLineStep',
            'cc::MainThreadScrollingReason',
            'cc::ManipulationInfo',
            'cc::ScrollOffsetAnimationCurve',
            'cc::ScrollSnapAlign',
            'cc::ScrollSnapType',
            'cc::ScrollStateData',
            'cc::ScrollUtils',
            'cc::SnapAlignment',
            'cc::SnapAreaData',
            'cc::SnapAxis',
            'cc::SnapContainerData',
            'cc::SnapFlingClient',
            'cc::SnapFlingController',
            'cc::SnappedTargetData',
            'cc::SnapPositionData',
            'cc::SnapSelectionStrategy',
            'cc::SnapStrictness',
            'cc::TargetSnapAreaElementIds',
            'ui::ScrollGranularity',

            # View transitions
            'cc::ViewTransitionContentLayer',
            'cc::ViewTransitionRequest',
            'viz::ViewTransitionElementResourceId',

            # base/types/strong_alias.h
            'base::StrongAlias',

            # Common display structs across display <-> Blink.
            'display::ScreenInfo',
            'display::ScreenInfos',

            # Terminal value for display id's used across display <-> Blink.
            'display::kInvalidDisplayId',

            # Standalone utility libraries that only depend on //base
            'skia::.+',
            'url::.+',

            # Nested namespaces under the blink namespace
            '[a-z_]+_names::.+',
            'bindings::.+',
            'canvas_heuristic_parameters::.+',
            'compositor_target_property::.+',
            'cors::.+',
            'css_parsing_utils::.+',
            'cssvalue::.+',
            'element_locator::.+',
            'encoding::.+',
            'encoding_enum::.+',
            'event_handling_util::.+',
            'event_util::.+',
            'file_error::.+',
            'file_system_access_error::.+',
            'geometry_util::.+',
            'inspector_\\w+_event::.+',
            'inspector_async_task::.+',
            'inspector_set_layer_tree_id::.+',
            'inspector_tracing_started_in_frame::.+',
            'keywords::.+',
            'layered_api::.+',
            'layout_invalidation_reason::.+',
            'layout_text_control::.+',
            'media_constraints_impl::.+',
            'media_element_parser_helpers::.+',
            'network_utils::.+',
            'origin_trials::.+',
            'paint_filter_builder::.+',
            'root_scroller_util::.+',
            'scheduler::.+',
            'scroll_customization::.+',
            'scroll_into_view_util::.+',
            'scroll_timeline_util::.+',
            'shadow_element_utils::.+',
            'style_change_extra_data::.+',
            'style_change_reason::.+',
            'svg_path_parser::.+',
            'touch_action_util::.+',
            'trace_event::.+',
            'unicode::.+',
            'v8_compile_hints::.+',
            'vector_math::.+',
            'web_core_test_support::.+',
            'worker_pool::.+',
            'xpath::.+',

            # Third-party libraries that don't depend on non-Blink Chrome code
            # are OK.
            'icu::.+',
            'inspector_protocol_encoding::.+',
            'perfetto::.+',  # tracing
            'snappy::.+',
            'testing::.+',  # googlemock / googletest
            'v8::.+',
            'v8_inspector::.+',

            # Inspector instrumentation and protocol
            'probe::.+',
            'protocol::.+',

            # Blink code shouldn't need to be qualified with the Blink namespace,
            # but there are exceptions, e.g. traits for Mojo.
            'blink::.+',

            # Assume that identifiers where the first qualifier is internal are
            # nested in the blink namespace.
            'internal::.+',

            # HTTP structured headers
            'net::structured_headers::.+',

            # CanonicalCookie and related headers
            'net::CanonicalCookie',
            'net::CookieInclusionStatus',
            'net::CookiePartitionKey',
            'net::CookiePriority',
            'net::CookieSameSite',
            'net::CookieSourceScheme',

            # HTTP status codes
            'net::ERR_.*',
            'net::HTTP_.+',

            # For ConnectionInfo enumeration
            'net::HttpConnectionInfo',

            # Network service.
            'network::.+',

            # Used in network service types.
            'net::SchemefulSite',
            'net::SiteForCookies',

            # Storage Access API metadata
            'net::StorageAccessApiStatus',

            # PartitionAlloc
            'partition_alloc::.+',

            # Some test helpers live in the blink::test namespace.
            'test::.+',

            # Some test helpers that live in the blink::frame_test_helpers
            # namespace.
            'frame_test_helpers::.+',

            # Blink uses Mojo, so it needs mojo::Receiver, mojo::Remote, et
            # cetera, as well as generated Mojo bindings.
            # Note that the Mojo callback helpers are explicitly forbidden:
            # Blink already has a signal for contexts being destroyed, and
            # other types of failures should be explicitly signalled.
            '(?:.+::)?mojom::.+',
            'mojo::(?!WrapCallback).+',
            'mojo_base::BigBuffer.*',
            'service_manager::InterfaceProvider',

            # STL containers such as std::string and std::vector are discouraged
            # but still needed for interop with blink/common. Note that other
            # STL types such as std::unique_ptr are encouraged.
            # Discouraged usages for data members are checked in clang plugin.
            'std::.+',

            # Similarly, GURL is allowed to interoperate with blink/common and
            # other common code shared between browser and renderer.
            # Discouraged usages for data members are checked in clang plugin.
            'GURL',

            # UI Cursor
            'ui::Cursor',

            # UI Pointer and Hover
            'ui::HOVER_TYPE_.*',
            'ui::HoverType',
            'ui::POINTER_TYPE_.*',
            'ui::PointerType',

            # UI Keyconverter
            'ui::DomCode',
            'ui::DomKey',
            'ui::KeycodeConverter',

            # Accessibility base types and the non-Blink enums they
            # depend on.
            'ax::mojom::BoolAttribute',
            'ax::mojom::HasPopup',
            'ax::mojom::Restriction',
            'ax::mojom::State',
            'ui::AXActionData',
            'ui::AXEvent',
            'ui::AXEventIntent',
            'ui::AXMode',
            'ui::AXNodeData',
            'ui::AXRelativeBounds',
            'ui::AXTreeChecks',
            'ui::AXTreeData',
            'ui::AXTreeID',
            'ui::AXTreeIDUnknown',
            'ui::AXTreeSerializer',
            'ui::AXTreeSource',
            'ui::AXTreeUpdate',
            'ui::kAXModeBasic',
            'ui::kAXModeComplete',
            'ui::kFirstGeneratedRendererNodeID',
            'ui::kInvalidAXNodeID',
            'ui::kLastGeneratedRendererNodeID',
            'ui::ToString',

            # Accessibility helper functions - mostly used in Blink for
            # serialization. Please keep alphabetized.
            'ui::CanHaveInlineTextBoxChildren',
            'ui::IsCellOrTableHeader',
            'ui::IsClickable',
            'ui::IsComboBox',
            'ui::IsComboBoxContainer',
            'ui::IsContainerWithSelectableChildren',
            'ui::IsDialog',
            'ui::IsEmbeddingElement',
            'ui::IsHeading',
            'ui::IsPlainContentElement',
            'ui::IsLandmark',
            'ui::IsPlatformDocument',
            'ui::IsPresentational',
            'ui::IsSelectRequiredOrImplicit',
            'ui::IsStructure',
            'ui::IsTableLike',
            'ui::IsTableRow',
            'ui::IsTableHeader',
            'ui::IsText',
            'ui::IsTextField',

            # Blink uses UKM for logging e.g. always-on leak detection (crbug/757374)
            'ukm::.+',

            # Permit using crash keys inside Blink without jumping through
            # hoops.
            'crash_reporter::.*CrashKey.*',

            # Useful for platform-specific code.
            'base::apple::(CFToNSPtrCast|NSToCFPtrCast|CFToNSOwnershipCast|NSToCFOwnershipCast)',
            'base::apple::ScopedCFTypeRef',
            'base::mac::MacOSVersion',
            'base::mac::MacOSMajorVersion',

            # Protected memory
            'base::ProtectedMemory',
            'base::ProtectedMemoryInitializer',
            'base::AutoWritableMemory',

            # Allow Highway SIMD Library and the possible namespace aliases.
            'hwy::HWY_NAMESPACE.*',
            'hw::.+',
        ],
        'disallowed': [
            ('base::Bind(Once|Repeating)',
             'Use WTF::BindOnce or WTF::BindRepeating.'),
            'base::BindPostTaskToCurrentDefault',
            _DISALLOW_NON_BLINK_MOJOM,
            _DISALLOW_CONTINUATION_DATA_,
        ],
        # These task runners are generally banned in blink to ensure
        # that blink tasks remain properly labeled. See
        # //third_party/blink/renderer/platform/scheduler/TaskSchedulingInBlink.md
        # for more.
        'inclass_disallowed': [
            'base::(SingleThread|Sequenced)TaskRunner::(GetCurrentDefault|CurrentDefaultHandle)'
        ],
    },
    {
        'paths': ['third_party/blink/renderer/bindings/'],
        'allowed': ['gin::.+'],
    },
    {
        'paths': [
            'third_party/blink/renderer/bindings/core/v8/serialization/',
            'third_party/blink/renderer/core/typed_arrays/',
        ],
        'allowed': ['base::BufferIterator'],
    },
    {
        'paths': [
            'third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h'
        ],
        'allowed': ['base::FastHash'],
    },
    {
        'paths':
        ['third_party/blink/renderer/bindings/core/v8/script_streamer.cc'],
        'allowed': [
            # For the script streaming to be able to block when reading from a
            # mojo datapipe.
            'base::BlockingType',
            'base::ScopedAllowBaseSyncPrimitives',
            'base::ScopedBlockingCall',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.cc'
        ],
        'allowed': [
            # For memory reduction histogram.
            'base::ProcessMetrics',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/controller/oom_intervention_impl.cc'],
        'allowed': [
            'base::BindOnce',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/controller/user_level_memory_pressure_signal_generator.cc'
        ],
        'allowed': [
            'base::MemoryPressureListener',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/animation'],
        'allowed': [
            '[a-z_]+_functions::.+',
            'cc::ScrollTimeline',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/animation_frame',
            'third_party/blink/renderer/core/offscreencanvas',
            'third_party/blink/renderer/core/html/canvas'
        ],
        'allowed': [
            'viz::BeginFrameArgs',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/annotation'],
        'allowed': [
            # AnnotationAgentContainerImpl reuses TextFragmentSelectorGenerator
            # and the callback must accept this type as the result code.
            'shared_highlighting::LinkGenerationError',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/offscreencanvas'],
        'allowed': [
            # Flags to be used to set up sharedImage
            'gpu::SHARED_IMAGE_USAGE_DISPLAY_READ',
            'gpu::SHARED_IMAGE_USAGE_SCANOUT',
            'gpu::SharedImageUsageSet'
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core',
            'third_party/blink/public/common/messaging/accelerated_image_info.h',
            'third_party/blink/public/common/messaging/accelerated_static_bitmap_image_mojom_traits.h',
            'third_party/blink/common/messaging/accelerated_static_bitmap_image_mojom_traits.cc'
        ],
        'allowed': [
            'gpu::ExportedSharedImage', 'gpu::SHARED_IMAGE_USAGE_DISPLAY_READ',
            'gpu::SHARED_IMAGE_USAGE_SCANOUT',
            'gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE',
            'gpu::SharedImageUsageSet',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/exported',
            'third_party/blink/renderer/core/frame',
        ],
        'allowed': [
            'cc::ActiveFrameSequenceTrackers',
            'cc::ApplyViewportChangesArgs',
            'cc::ContentLayerClient',
            'cc::DeadlinePolicy',
            'cc::DisplayItemList',
            'cc::DrawColorOp',
            'cc::DrawImageOp',
            'cc::LayerTreeSettings',
            'cc::PaintBenchmarkResult',
            'cc::RenderFrameMetadata',
            'cc::RestoreOp',
            'cc::SaveOp',
            'cc::ScaleOp',
            'cc::TaskGraphRunner',
            'cc::TranslateOp',
            'gfx::DisplayColorSpaces',
            'gfx::FontRenderParams',
            'ui::ImeTextSpan',
            'viz::FrameSinkId',
            'viz::LocalSurfaceId',
            'viz::SurfaceId',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/exported/web_form_control_element.cc'
        ],
        'allowed': [
            'base::i18n::LEFT_TO_RIGHT',
            'base::i18n::RIGHT_TO_LEFT',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/exported/web_view_impl.cc'],
        'allowed': [
            'base::TaskAnnotator',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/clipboard'],
        'allowed': ['base::EscapeForHTML'],
    },
    {
        'paths': ['third_party/blink/renderer/core/css'],
        'allowed': [
            # Internal implementation details for CSS.
            'css_property_parser_helpers::.+',
            'detail::.+',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/css/color_function.h'],
        'allowed': [
            'base::MakeFixedFlatMap',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/css/media_values.cc'],
        'allowed': [
            'color_space_utilities::GetColorSpaceGamut',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/css/style_color.cc'],
        'allowed': [
            'base::flat_map',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/editing/ime'],
        'allowed': [
            'ui::ImeTextSpan',
            'ui::TextInputAction',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/editing/commands/undo_stack.cc',
            'third_party/blink/renderer/core/editing/commands/undo_stack.h'
        ],
        'allowed': [
            'base::MemoryPressureListener',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/fetch/data_consumer_handle_test_util.cc'
        ],
        'allowed': [
            # The existing code already contains gin::IsolateHolder.
            'gin::IsolateHolder',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/frame/deprecation/deprecation.cc'],
        'allowed': [
            'base::CommandLine',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/core/frame/visual_viewport.cc',
            'third_party/blink/renderer/core/frame/visual_viewport.h'
        ],
        'allowed': [
            'cc::SolidColorScrollbarLayer',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/frame/web_frame_widget_impl.cc'],
        'allowed': [
            'cc::CompositorCommitData',
            'cc::InputHandlerScrollResult',
            'cc::SwapPromise',
            'viz::CompositorFrameMetadata',
            'viz::FrameTimingDetails',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/frame/web_frame_widget_impl.h'],
        'allowed': [
            'cc::CompositorCommitData',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/frame/web_local_frame_impl.cc'],
        'allowed': [
            'ui::AXTreeID',
            'ui::AXTreeIDUnknown',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/frame/web_local_frame_impl.h'],
        'allowed': [
            'ui::AXTreeID',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/fileapi/file_reader_loader.cc',
            'third_party/blink/renderer/modules/file_system_access/file_system_underlying_sink.cc'
        ],
        'allowed': [
            'net::ERR_.+',
            'net::OK',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/html/forms'],
        'allowed': [
            'ui::TextInputType',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/loader/alternate_signed_exchange_resource_info.cc'
        ],
        'allowed': [
            # Used by WebPackageRequestMatcher in //third_party/blink/common.
            'net::HttpRequestHeaders',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/loader/web_bundle/script_web_bundle.cc'
        ],
        'allowed': [
            'web_package::ScriptWebBundleOriginType',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/paint'],
        'allowed': [
            # cc painting types.
            'cc::ContentLayerClient',
            'cc::DisplayItemList',
            'cc::DrawRecordOp',

            # blink paint tree debugging namespace
            'paint_property_tree_printer::UpdateDebugNames',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/fragment_directive'],
        'allowed': [
            'cc::ScrollbarLayerBase',
            'shared_highlighting::.+',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/page'],
        'allowed': [
            'touch_adjustment::.+',
            'viz::FrameSinkId',
        ],
    },
    {
        'paths': [
            'third_party/blink/public/web/web_frame_widget.h',
            'third_party/blink/renderer/core/frame/animation_frame_timing_monitor',
            'third_party/blink/renderer/core/frame/local_frame_client_impl.cc',
            'third_party/blink/renderer/core/frame/web_frame_widget',
            'third_party/blink/renderer/core/page/chrome_client.h',
            'third_party/blink/renderer/core/paint/timing',
            'third_party/blink/renderer/core/timing',
        ],
        'allowed': [
            'viz::FrameTimingDetails',
        ],
    },
    {
        'paths': [
            'third_party/blink/public/',
        ],
        'allowed': [
            'gfx::Insets',
            'gfx::Point',
            'gfx::PointF',
            'gfx::Rect',
            'gfx::RectF',

            # The Blink public API is shared between non-Blink and Blink code
            # and must use the regular variants.
            'mojom::.+',
            'ui::mojom::WindowShowState',
            'ui::mojom::WindowShowState::.+',

            # Metadata for the Storage Access API.
            'net::StorageAccessApiStatus',

            # Prefer WebString over std::string in the public API. Other STL
            # types are generally allowed for interop with non-Blink code, as
            # containers like WTF::Vector are not exposed outside Blink.
            'std::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/public/common',
        ],
        'allowed': [
            # Blink code shouldn't need to be qualified with the Blink namespace,
            # but there are exceptions, e.g. traits for Mojo.
            'blink::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/public/platform/web_audio_device.h',
        ],
        'allowed': [
            'media::OutputDeviceStatus',
        ],
    },
    {
        'paths': [
            'third_party/blink/public/web/web_dom_activity_logger.h',
        ],
        'allowed': [
            'v8::Local',
            'v8::Value',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.cc',
            'third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h',
        ],
        'allowed': [
            'base::DelayedTaskHandle',
            # Temporarily added to generate the value of a crash key.
            'base::NumberToString',
            'base::subtle::PostDelayedTaskPassKey',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/style/computed_style.h'],
        'allowed': [
            'css_longhand::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/inspector/inspector_memory_agent.cc',
            'third_party/blink/renderer/core/inspector/inspector_memory_agent.h',
        ],
        'allowed': [
            'base::ModuleCache',
            'base::PoissonAllocationSampler',
            'base::SamplingHeapProfiler',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/inspector/inspector_overlay_agent.cc'
        ],
        'allowed': [
            # cc painting types.
            'cc::ContentLayerClient',
            'cc::DisplayItemList',
            'cc::DrawRecordOp',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/css/properties/css_parsing_utils.cc',
            'third_party/blink/renderer/core/html/html_permission_element.cc',
            'third_party/blink/renderer/core/paint/box_border_painter.cc',
        ],
        'allowed': [
            'color_utils::GetContrastRatio',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/platform/graphics/compositing/pending_layer.cc',
            'third_party/blink/renderer/platform/graphics/paint/paint_chunker.cc',
        ],
        'allowed': [
            'color_utils::GetResultingPaintColor',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/inspector/inspector_contrast.cc',
            'third_party/blink/renderer/core/inspector/inspector_contrast.h'
        ],
        'allowed': [
            'color_utils::GetContrastRatio',
            'cc::RTree',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/inspector/locale_controller.cc'],
        'allowed': [
            'base::i18n::SetICUDefaultLocale',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/inspector'],
        'allowed': [
            # [C]h[R]ome [D]ev[T]ools [P]rotocol implementation support library
            # (see third_party/inspector_protocol/crdtp).
            'crdtp::.+',
            # DevTools manages certificates from the net stack.
            'net::X509Certificate',
            'net::x509_util::CryptoBufferAsSpan',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/inspector',
            'third_party/blink/renderer/controller/dev_tools_frontend_impl.h',
            'third_party/blink/renderer/controller/dev_tools_frontend_impl.cc',
            'third_party/blink/renderer/modules/filesystem/dev_tools_host_file_system.cc'
        ],
        'allowed': [
            # Commands from the DevTools window are parsed from a JSON string in
            # the devtools renderer and sent on as base::Value.
            'base::Value',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/inspector/dev_tools_host.cc'],
        'allowed': [
            # Commands from the DevTools window are parsed from a JSON string in
            # the devtools renderer and sent on as base::Value.
            'base::JSONReader',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/inspector/inspector_performance_agent.cc'
        ],
        'allowed': [
            'base::subtle::TimeTicksNowIgnoringOverride',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/inspector/inspector_network_agent.cc'
        ],
        'allowed': [
            'base::flat_set',
            'base::HexEncode',
            'net::ct::.+',
            'net::IPAddress',
            'net::SourceStream',
            'net::SSL.+',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/workers/worker_thread.cc'],
        'allowed': [
            'base::ScopedAllowBaseSyncPrimitives',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/loader/document_loader.cc',
            'third_party/blink/renderer/core/loader/document_loader.h',
        ],
        'allowed': [
            'base::flat_map',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/xml'],
        'allowed': [
            'xpathyy::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/device_orientation/',
            'third_party/blink/renderer/modules/gamepad/',
            'third_party/blink/renderer/modules/sensor/',
            'third_party/blink/renderer/modules/xr/',
        ],
        'allowed': [
            'base::subtle::Atomic32',
            'device::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/html/media/',
            'third_party/blink/renderer/modules/canvas/',
            'third_party/blink/renderer/modules/vr/',
            'third_party/blink/renderer/modules/webcodecs/',
            'third_party/blink/renderer/modules/webgl/',
            'third_party/blink/renderer/modules/webgpu/',
            'third_party/blink/renderer/modules/xr/',
        ],
        # The modules listed above need access to the following GL drawing and
        # display-related types.
        'allowed': [
            'base::flat_map',
            'display::Display',
            'gl::GpuPreference',
            'gpu::ClientSharedImage',
            'gpu::gles2::GLES2Interface',
            'gpu::Mailbox',
            'gpu::MailboxHolder',
            'gpu::raster::RasterInterface',
            'gpu::SHARED_IMAGE_USAGE_.+',
            'gpu::SharedImageInterface',
            'gpu::SharedImageUsageSet',
            'gpu::SyncToken',
            'gpu::webgpu::ReservedTexture',
            'media::IsOpaque',
            'media::kNoTransformation',
            'media::PaintCanvasVideoRenderer',
            'media::PIXEL_FORMAT_Y16',
            'media::VideoFrame',
            'viz::RasterContextProvider',
            'viz::ReleaseCallback',
            'viz::SinglePlaneFormat',
            'viz::ToClosestSkColorType',
            'viz::TransferableResource',
            'wgpu::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc',
        ],
        # This class needs access to a GPU driver bug workaround entry.
        'allowed': [
            'gpu::ENABLE_WEBGL_TIMER_QUERY_EXTENSIONS',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h',
        ],
        # This class needs to pass gpu::Capabilities() to a //media function.
        'allowed': [
            'gpu::Capabilities',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/html/media/',
        ],
        # This module needs access to the following for media's base::Feature
        # list.
        'allowed': [
            'media::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/core/timing/background_tracing_helper.cc',
            'third_party/blink/renderer/core/timing/background_tracing_helper.h',
        ],
        'allowed': [
            'base::MD5Digest',
            'base::MD5Sum',
            'base::SPLIT_WANT_NONEMPTY',
            'base::SplitStringPiece',
            'base::StringPiece',
            'base::TRIM_WHITESPACE',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/breakout_box/',
        ],
        'allowed': [
            # Required to initialize WebGraphicsContext3DVideoFramePool.
            'gpu::GpuMemoryBufferManager',
            'media::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/mediasource/',
        ],
        'allowed': [
            'base::CommandLine',
            'media::.+',
            'switches::kLacrosEnablePlatformEncryptedHevc',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/encryptedmedia/',
            'third_party/blink/renderer/modules/media/',
            'third_party/blink/renderer/modules/video_rvfc/',
        ],
        'allowed': [
            'media::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/media_capabilities/',
        ],
        'allowed': [
            'media::.+',
            'media_capabilities_identifiability_metrics::.+',
            'webrtc::SdpVideoFormat',
            'webrtc::SdpAudioFormat',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/media/audio/',
        ],
        'allowed': [
            # TODO(https://crbug.com/787252): Remove most of the entries below,
            # once the directory is fully Onion soup'ed.
            'base::Bind.*',
            'base::EraseIf',
            'base::flat_map',
            'base::flat_set',
            'base::NoDestructor',
            'base::RetainedRef',
            'base::ScopedPlatformFile',
            'base::Unretained',
            'mojo::WrapCallbackWithDefaultInvokeIfNotRun',

            # TODO(https://crrev.com/787252): Consider allowlisting fidl::*
            # usage more broadly in Blink.
            'fidl::InterfaceHandle',
        ],
        'inclass_allowed': ['base::SequencedTaskRunner::GetCurrentDefault']
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/imagecapture/',
        ],
        'allowed': [
            'cc::SkiaPaintCanvas',
            'libyuv::.+',
            'media::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/media_capabilities/media_capabilities_fuzzer.cc',
        ],
        'allowed': [
            'mc_fuzzer::.+',
            'google::protobuf::RepeatedField',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/mediacapturefromelement/',
        ],
        'allowed': [
            'gpu::MailboxHolder',
            'media::.+',
            'libyuv::.+',
            'viz::SkColorTypeToSinglePlaneSharedImageFormat',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/mediarecorder/',
        ],
        'allowed': [
            'base::ClampMul',
            'base::MakeFixedFlatMap',
            'base::NumberToString',
            # TODO(crbug.com/960665): Remove base::queue once it is replaced with a WTF equivalent.
            'base::queue',
            'base::SharedMemory',
            'base::StringPiece',
            'base::ThreadTaskRunnerHandle',
            'libopus::.+',
            'libyuv::.+',
            'media::.+',
            'video_track_recorder::.+',
        ],
        'inclass_allowed': [
            'base::(SingleThread|Sequenced)TaskRunner::(CurrentDefaultHandle|GetCurrentDefault)'
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/mediastream/',
        ],
        'allowed': [
            'media::.+',
            'base::Hash',
            'base::Lock',
            'base::StringPrintf',
            'base::TaskRunner',
            # TODO(crbug.com/704136): Switch to using frame-based task runners.
            'base::ThreadTaskRunnerHandle',
            'base::subtle::Atomic32',
            'base::subtle::Acquire_Load',
            'base::subtle::NoBarrier_.+',
            'base::subtle::Release_Store',
            'cc::SkiaPaintCanvas',
            'cc::UpdateSubmissionStateCB',
            'cc::VideoFrameProvider',
            'cc::VideoLayer',
            'gpu::gles2::GLES2Interface',
            'libyuv::.+',
            'media_constraints::.+',
            "rtc::RefCountedObject",
            'rtc::TaskQueue',
            'rtc::scoped_refptr',
            'viz::.+',
            'webrtc::Aec3ConfigFromJsonString',
            'webrtc::AudioProcessingBuilder',
            'webrtc::AudioProcessing',
            'webrtc::AudioProcessorInterface',
            'webrtc::AudioTrackInterface',
            'webrtc::Config',
            'webrtc::EchoCanceller3Config',
            'webrtc::EchoCanceller3Factory',
            'webrtc::ExperimentalAgc',
            'webrtc::ExperimentalNs',
            'webrtc::MediaStreamTrackInterface',
            'webrtc::ObserverInterface',
            'webrtc::StreamConfig',
            'webrtc::TypingDetection',
            'webrtc::VideoTrackInterface',
        ],
        'inclass_allowed': [
            'base::(SingleThread|Sequenced)TaskRunner::(CurrentDefaultHandle|GetCurrentDefault)'
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/indexeddb/',
        ],
        'allowed': [
            'indexed_db::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/remote_objects/',
        ],
        'allowed': [
            'gin::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webcodecs/',
        ],
        'allowed': [
            'base::ClampMul',
            'base::DoNothingWithBoundArgs',
            'base::IsAligned',
            'base::PlatformThreadRef',
            'base::WrapRefCounted',
            'cc::kNumYUVPlanes',
            'cc::SkiaPaintCanvas',
            'cc::YUVIndex',
            'cc::YUVSubsampling',
            'gpu::kNullSurfaceHandle',
            'gpu::Mailbox',
            'gpu::MailboxHolder',
            'gpu::raster::RasterInterface',
            'gpu::SHARED_IMAGE_.+',
            'gpu::SharedImageInterface',
            'gpu::SharedImageUsageSet',
            'gpu::SyncToken',
            'libgav1::.+',
            'libyuv::.+',
            'media::.+',
            'viz::RasterContextProvider',
            'viz::ReleaseCallback',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webcodecs/video_decoder_fuzzer.cc',
            'third_party/blink/renderer/modules/webcodecs/fuzzer_utils.cc',
            'third_party/blink/renderer/modules/webcodecs/fuzzer_utils.h',
        ],
        'allowed': [
            'wc_fuzzer::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webgpu/',
        ],
        'allowed': [
            'base::CommandLine',
            'switches::kEnableUnsafeWebGPU',
            # The WebGPU Blink module needs access to the WebGPU control
            # command buffer interface.
            'gpu::webgpu::PowerPreference',
            'gpu::webgpu::WebGPUInterface',
            'media::PIXEL_FORMAT_NV12',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webrtc/',
        ],
        'allowed': [
            'base::Erase',
            'base::Lock',
            'base::StringPrintf',
            'media::.+',
            'rtc::scoped_refptr',
            'webrtc::AudioDeviceModule',
            'webrtc::AudioParameters',
            'webrtc::AudioSourceInterface',
            'webrtc::AudioTransport',
            'webrtc::kAdmMaxDeviceNameSize',
            'webrtc::kAdmMaxGuidSize',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webshare/',
        ],
        'allowed': [
            'base::SafeBaseName',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/platform/',
        ],
        # Suppress almost all checks on platform since code in this directory is
        # meant to be a bridge between Blink and non-Blink code. However,
        # base::RefCounted and base::RefCountedThreadSafe should still be
        # explicitly blocked.
        # WTF::RefCounted and WTF::ThreadSafeRefCounted should be used instead.
        'allowed': ['.+'],
        'inclass_allowed': ['.+'],
        'disallowed': [
            ('base::RefCounted', 'Use RefCounted'),
            ('base::RefCountedThreadSafe', 'Use ThreadSafeRefCounted'),
            # TODO(https://crbug.com/1267866): this warning is shown twice for
            # renderer/platform/ violations.
            _DISALLOW_NON_BLINK_MOJOM,
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/platform/wtf/',
        ],
        # base::RefCounted and base::RefCountedThreadSafe are prohibited in
        # platform/ as defined above, but wtf/ needs them to define their WTF
        # subclasses and CrossThreadCopier.
        'allowed': ['base::RefCounted', 'base::RefCountedThreadSafe'],
    },
    {
        'paths': [
            'third_party/blink/renderer/platform/scheduler/common/single_thread_idle_task_runner.h',
        ],
        # base::RefCounted is prohibited in platform/ as defined above, but
        # SingleThreadIdleTaskRunner needs to be constructed before WTF and
        # PartitionAlloc are initialized, which forces us to use
        # base::RefCountedThreadSafe for it.
        'allowed': ['base::RefCountedThreadSafe'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/exported/',
            'third_party/blink/renderer/core/frame/',
            'third_party/blink/renderer/core/input/',
        ],
        'allowed': [
            'ui::LatencyInfo',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/accessibility/',
        ],
        'allowed': [
            'base::MakeFixedFlatSet',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/animationworklet/',
        ],
        'allowed': [
            'cc::AnimationOptions',
            'cc::AnimationEffectTimings',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/url_pattern/',
        ],
        'allowed': [
            # Required to provide a canonicalization functor to liburlpattern.
            "absl::InvalidArgumentError",
            "absl::Status",
            "absl::StatusOr",

            # Needed to work with std::string values returned from
            # liburlpattern API.
            "base::IsStringASCII",

            # Needed to use part of the StringUTF8Adaptor API.
            "base::StringPiece",

            # //third_party/liburlpattern
            'liburlpattern::.+',

            # Internal namespace used by url_pattern module.
            'url_pattern::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webaudio/',
        ],
        'allowed': ['audio_utilities::.+', 'media::.+'],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webdatabase/',
        ],
        'allowed': ['sql::.+'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/layout/layout_theme.cc',
            'third_party/blink/renderer/core/layout/layout_theme_mac.mm',
            'third_party/blink/renderer/core/paint/outline_painter.cc',
            'third_party/blink/renderer/core/paint/theme_painter.cc',
            'third_party/blink/renderer/core/paint/theme_painter_default.cc',
        ],
        'allowed': ['ui::NativeTheme.*', 'ui::color_utils.*'],
    },
    {
        'paths': [
            'third_party/blink/public/platform/web_theme_engine.h',
            'third_party/blink/renderer/core/css/resolver/style_builder_converter.h',
            'third_party/blink/renderer/core/layout/layout_theme.cc',
            'third_party/blink/renderer/core/layout/layout_theme.h',
            'third_party/blink/renderer/core/scroll/',
        ],
        'allowed': ['ui::ColorProvider'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/scroll/mac_scrollbar_animator_impl.h',
            'third_party/blink/renderer/core/scroll/mac_scrollbar_animator_impl.mm',
        ],
        'allowed': [
            'ui::ScrollbarAnimationTimerMac',
            'ui::OverlayScrollbarAnimatorMac',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/crypto/',
        ],
        'allowed': ['crypto::.+'],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/p2p',
        ],
        'allowed': [
            # TODO(crbug.com/787254): Remove GURL usage.
            'GURL',
            'cricket::.*',
            'rtc::.+',
        ]
    },
    {
        'paths': [
            # TODO(crbug.com/787254): Separate the two paths below and their own
            # allowlist.
            'third_party/blink/renderer/modules/peerconnection/',
            'third_party/blink/renderer/bindings/modules/v8/serialization/',
        ],
        'allowed': [
            'absl::.+',
            # TODO(crbug.com/1266408): Temporarily added to enable splitting UMA stats based on tier.
            'base::CPU',
            'base::LazyInstance',
            'base::Lock',
            # TODO(crbug.com/787254): Remove base::BindOnce, base::Unretained,
            # base::OnceClosure, base::RepeatingClosure, base::CurrentThread and
            # base::RetainedRef.
            'base::Bind.*',
            'base::MD5.*',
            'base::CurrentThread',
            'base::.*Closure',
            'base::PowerObserver',
            'base::RetainedRef',
            'base::StringPrintf',
            'base::Value',
            'base::Unretained',
            # TODO(crbug.com/787254): Replace base::Thread with the appropriate Blink class.
            'base::Thread',
            'base::WrapRefCounted',
            'cricket::.*',
            'webrtc::ThreadWrapper',
            # TODO(crbug.com/787254): Remove GURL usage.
            'GURL',
            'media::.+',
            'net::NetworkTrafficAnnotationTag',
            'net::DefineNetworkTrafficAnnotation',
            # TODO(crbug.com/1266408): Temporarily added to enable splitting UMA stats based on tier.
            're2::RE2',
            'rtc::.+',
            'webrtc::.+',
            'quic::.+',
            'quiche::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/peerconnection/adapters/',
        ],
        # The code in adapters/ wraps WebRTC APIs using STL/WebRTC types only.
        # Thus, the restriction that objects should only be created and
        # destroyed on the same thread can be relaxed since no Blink types (like
        # AtomicString or HeapVector) are used cross thread. These Blink types
        # are converted to the STL/WebRTC counterparts in the parent directory.
        'allowed': [
            'base::OnTaskRunnerDeleter',
            'sigslot::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/direct_sockets/',
        ],
        'allowed': [
            'net::DefineNetworkTrafficAnnotation',
            'net::Error',
            'net::MutableNetworkTrafficAnnotationTag',
            'net::NetworkTrafficAnnotationTag',
        ]
    },
    {
        'paths': ['third_party/blink/renderer/modules/manifest/'],
        'allowed': [
            'net::IsValidTopLevelMimeType',
            'net::ParseMimeTypeWithoutParameter',
            'net::registry_controlled_domains::.+',

            # Needed to use the liburlpattern API.
            "absl::StatusOr",
            'liburlpattern::.+',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/fetch/fetch_request_data.cc'],
        'allowed': ['net::RequestPriority'],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/fetch/fetch_response_data.cc'],
        'allowed': [
            'storage::ComputeRandomResponsePadding',
            'storage::ComputeStableResponsePadding',
            'storage::ShouldPadResponseType'
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/filesystem/dom_file_system.cc',
            'third_party/blink/renderer/modules/webdatabase/database_tracker.cc',
        ],
        'allowed': [
            'storage::GetIdentifierFromOrigin',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/frame/local_frame.cc',
            'third_party/blink/renderer/core/frame/local_frame.h',
        ],
        'allowed': [
            'gfx::ImageSkia',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/frame/local_frame_view.cc'],
        'allowed': [
            'base::LapTimer',
            'cc::frame_viewer_instrumentation::IsTracingLayerTreeSnapshots',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webaudio/offline_audio_worklet_thread.cc',
            'third_party/blink/renderer/modules/webaudio/realtime_audio_worklet_thread.cc',
            'third_party/blink/renderer/modules/webaudio/semi_realtime_audio_worklet_thread.cc',
        ],
        'allowed': ['base::ThreadType'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/frame/local_frame_mojo_handler.cc',
            'third_party/blink/renderer/core/frame/local_frame_mojo_handler.h',
            'third_party/blink/renderer/core/frame/pausable_script_executor.cc',
        ],
        # base::Value is used as a part of script evaluation APIs.
        'allowed': ['base::Value'],
    },
    {
        'paths': ['third_party/blink/renderer/core/frame/local_dom_window.cc'],
        'allowed': [
            'net::registry_controlled_domains::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/platform/audio/',
            'third_party/blink/renderer/modules/webaudio/',
        ],
        'allowed': ['fdlibm::.+'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/html/canvas/html_canvas_element.cc',
            'third_party/blink/renderer/core/html/canvas/html_canvas_element.h',
        ],
        'allowed': ['viz::ResourceId'],
    },
    {
        'paths': [
            'third_party/blink/renderer/platform/graphics/view_transition_element_id.h'
        ],
        'allowed': ['cc::ViewTransitionElementId'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/view_transition/',
        ],
        'allowed': ['base::flat_map', 'cc::ScopedPauseRendering'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/',
        ],
        'allowed': ['ui::k200Percent'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.cc',
        ],
        'allowed': [
            'base::NoDestructor',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/core/loader/frame_loader.cc',
        ],
        'allowed': [
            'base::flat_map',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webdatabase/dom_window_web_database.cc',
        ],
        'allowed': [
            'base::CommandLine',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/controller/blink_leak_detector.cc',
        ],
        'allowed': [
            'base::CommandLine',
            'switches::kEnableLeakDetectionHeapSnapshot',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/controller/blink_shutdown.cc',
        ],
        'allowed': [
            'base::CommandLine',
            'switches::kDumpRuntimeCallStats',
        ]
    },
    {
        'paths':
        ['third_party/blink/renderer/bindings/core/v8/local_window_proxy.cc'],
        'allowed': [
            'base::SingleSampleMetric',
            'base::SingleSampleMetricsFactory',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/service_worker/navigation_preload_request.cc',
            'third_party/blink/renderer/modules/service_worker/navigation_preload_request.h',
        ],
        'allowed': [
            'net::ERR_.+',
            'net::HttpResponseHeaders',
            'net::OK',
            'net::RedirectInfo',
        ],
    },
    {
        # base::Value is used in test-only script execution in worker contexts.
        'paths': [
            'third_party/blink/renderer/modules/service_worker/service_worker_global_scope.cc',
        ],
        'allowed': [
            'base::Value',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/modules/clipboard/'],
        'allowed': [
            'net::ParseMimeTypeWithoutParameter',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/mediastream/media_stream_track.cc',
        ],
        'allowed': [
            # Used for injecting a mock.
            'base::NoDestructor',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/browsing_topics/browsing_topics_document_supplement.cc',
        ],
        'allowed': [
            'browsing_topics::ApiAccessResult',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/core/frame/attribution_src_loader.cc',
            'third_party/blink/renderer/core/frame/attribution_src_loader.h',
        ],
        'allowed': [
            'attribution_reporting::.*',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/core/origin_trials/origin_trial_context.cc',
        ],
        'allowed': [
            'attribution_reporting::features::.*',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/core/highlight/highlight_style_utils.cc',
        ],
        'allowed': [
            'shared_highlighting::kFragmentTextBackgroundColorARGB',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/core/events/keyboard_event.h',
            'third_party/blink/renderer/core/events/keyboard_event.cc',
        ],
        'allowed': [
            'base::StringPiece16',
            'base::i18n::UTF16CharIterator',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/service_worker/service_worker_router_type_converter_test.cc',
            # TODO(crbug.com/1371756): consolidate code using liburlpattern.
            # Especially, consolidate manifest and this code.
        ],
        'allowed': [
            'liburlpattern::Parse',
            'liburlpattern::Part',
            'liburlpattern::PartType',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/remoteplayback/',
        ],
        'allowed': [
            'media::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/ml/webnn/',
            'third_party/blink/renderer/modules/ml/ml_context.cc',
            'third_party/blink/renderer/modules/ml/ml_context.h',
            'third_party/blink/renderer/modules/ml/ml_model_loader_test_util.cc',
        ],
        'allowed': [
            'blink_mojom::.+',
            'webnn::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/ad_auction/',
            'third_party/blink/renderer/modules/shared_storage/',
        ],
        'allowed': [
            'aggregation_service::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/core/scheduler/',
            'third_party/blink/renderer/modules/shared_storage/',
        ],
        'allowed': [
            _DISALLOW_CONTINUATION_DATA_[0],
        ]
    },
    {
        'paths': [
            'third_party/blink/public/common/permissions_policy/permissions_policy.h',
        ],
        'allowed': [
            'url::Origin',
        ]
    },
    {
        'paths': [
            'third_party/blink/public/common/tokens/',
        ],
        'allowed': [
            'base::TokenType',
        ]
    },
    {
        'paths': [
            'third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h',
        ],
        'allowed': [
            'gpu::SharedImageInterface',
        ]
    },
]


def _precompile_config():
    """Turns the raw config into a config of compiled regex."""
    match_nothing_re = re.compile('.^')

    def compile_regexp(match_list, is_list=True):
        """Turns a match list into a compiled regexp.

        If match_list is None, a regexp that matches nothing is returned.
        """
        if (match_list and is_list):
            match_list = '(?:%s)$' % '|'.join(match_list)
        if match_list:
            return re.compile(match_list)
        return match_nothing_re

    def compile_disallowed(disallowed_list):
        """Transforms the disallowed list to one with the regexps compiled."""
        if not disallowed_list:
            return match_nothing_re, []
        match_list = []
        advice_list = []
        for entry in disallowed_list:
            if isinstance(entry, tuple):
                warning = ''
                if len(entry) == 2:
                    match, advice = entry
                else:
                    match, advice, warning = entry
                match_list.append(match)
                advice_list.append((compile_regexp(match, False), advice,
                                    warning == 'Warning'))
            else:
                # Just a string
                match_list.append(entry)
        return compile_regexp(match_list), advice_list

    compiled_config = []
    for raw_entry in _CONFIG:
        disallowed, advice = compile_disallowed(raw_entry.get('disallowed'))
        inclass_disallowed, inclass_advice = compile_disallowed(
            raw_entry.get('inclass_disallowed'))
        compiled_config.append({
            'paths':
            raw_entry['paths'],
            'allowed':
            compile_regexp(raw_entry.get('allowed')),
            'disallowed':
            disallowed,
            'advice':
            advice,
            'inclass_allowed':
            compile_regexp(raw_entry.get('inclass_allowed')),
            'inclass_disallowed':
            inclass_disallowed,
            'inclass_advice':
            inclass_advice,
        })
    return compiled_config


_COMPILED_CONFIG = _precompile_config()

# Attempt to match identifiers qualified with a namespace. Since
# parsing C++ in Python is hard, this regex assumes that namespace
# names only contain lowercase letters, numbers, and underscores,
# matching the Google C++ style guide. This is intended to minimize
# the number of matches where :: is used to qualify a name with a
# class or enum name.
#
# As a bit of a minor hack, this regex also hardcodes a check for GURL, since
# GURL isn't namespace qualified and wouldn't match otherwise.
# ContinuationPreservedEmbedder data is similarly hardcoded to restrict access
# to the v8 APIs which would not otherwise match.
#
# An example of an identifier that will be matched with this RE is
# "base::BindOnce" or "performance_manager::policies::WorkingSetTrimData".
_IDENTIFIER_WITH_NAMESPACE_RE = re.compile(
    r'\b(?:(?:[a-z_][a-z0-9_]*::)+[A-Za-z_][A-Za-z0-9_]*|GURL|.*ContinuationPreservedEmbedderData.*)\b'
)

# Different check which matches a non-empty sequence of lower-case
# alphanumeric namespaces, followed by at least one
# upper-or-lower-case alphanumeric qualifier, followed by the
# upper-or-lower-case alphanumeric identifier.  This matches
# identifiers which are within a class.
#
# In case identifiers are matched by both _IDENTIFIER_IN_CLASS and
# _IDENTIFIER_WITH_NAMESPACE, the behavior of
# _IDENTIFIER_WITH_NAMESPACE takes precedence, and it is as if it was
# not matched by _IDENTIFIER_IN_CLASS.
#
# An example of an identifier matched by this regex is
# "base::SingleThreadTaskRunner::CurrentDefaultHandle".
_IDENTIFIER_IN_CLASS_RE = re.compile(
    r'\b(?:[a-z_][a-z0-9_]*::)+(?:[A-Za-z_][A-Za-z0-9_]*::)+[A-Za-z_][A-Za-z0-9_]*\b'
)


def _find_matching_entries(path):
    """Finds entries that should be used for path.

    Returns:
        A list of entries, sorted in order of relevance. Each entry is a
        dictionary with keys:
            allowed: A regexp for identifiers that should be allowed.
            disallowed: A regexp for identifiers that should not be allowed.
            advice: (optional) A regexp for identifiers along with advice
    """
    entries = []
    for entry in _COMPILED_CONFIG:
        for entry_path in entry['paths']:
            if path.startswith(entry_path):
                entries.append({'sortkey': len(entry_path), 'entry': entry})
    # The path length is used as the sort key: a longer path implies more
    # relevant, since that config is a more exact match.
    entries.sort(key=lambda x: x['sortkey'], reverse=True)
    return [entry['entry'] for entry in entries]


def _check_entries_for_identifier(entries, identifier, in_class=False):
    """Check if an identifier is allowed"""
    for entry in entries:
        if in_class:
            if entry['inclass_disallowed'].match(identifier):
                return False
            if entry['inclass_allowed'].match(identifier):
                return True
        else:
            if entry['disallowed'].match(identifier):
                return False
            if entry['allowed'].match(identifier):
                return True
    # Identifiers in classes which are allowed are allowed by default,
    # while non-inner identifiers are disallowed by default.
    return in_class


def _find_advice_for_identifier(entries, identifier, in_class=False):
    advice_list = []
    all_warning = True
    for entry in entries:
        for matcher, advice, warning in entry.get(
                'inclass_advice' if in_class else 'advice', []):
            if matcher.match(identifier):
                advice_list.append(advice)
                all_warning = all_warning and warning
    return advice_list, all_warning


class BadIdentifier(object):
    """Represents a single instance of a bad identifier."""

    def __init__(self, identifier, line, advice=None, warning=False):
        self.identifier = identifier
        self.line = line
        self.advice = advice
        self.warning = warning


def check(path, contents):
    """Checks for disallowed usage of non-Blink classes, functions, et cetera.

    Args:
        path: The path of the file to check.
        contents: An array of line number, line tuples to check.

    Returns:
        A list of (line number, disallowed identifier, advice) tuples.
    """
    results = []
    # Because Windows.
    path = path.replace('\\', '/')
    basename, ext = os.path.splitext(path)
    # Only check code. Ignore tests and fuzzers.
    if (ext not in ('.cc', '.cpp', '.h', '.mm') or path.find('/testing/') >= 0
            or path.find('/core/web_test/') >= 0 or path.find('/tests/') >= 0
            or basename.endswith('_test') or basename.endswith('_test_helpers')
            or basename.endswith('_test_utils')
            or basename.endswith('_unittest') or basename.endswith('_fuzzer')
            or basename.endswith('_perftest')):
        return results
    entries = _find_matching_entries(path)
    if not entries:
        return
    for line_number, line in contents:
        idx = line.find('//')
        if idx >= 0:
            line = line[:idx]
        identifiers = set(_IDENTIFIER_WITH_NAMESPACE_RE.findall(line))
        in_class_identifiers = set(_IDENTIFIER_IN_CLASS_RE.findall(line))
        in_class_identifiers -= identifiers
        for identifier in identifiers:
            if not _check_entries_for_identifier(entries, identifier):
                advice, warning = _find_advice_for_identifier(
                    entries, identifier)
                results.append(
                    BadIdentifier(identifier, line_number, advice, warning))
        for identifier in in_class_identifiers:
            if not _check_entries_for_identifier(entries, identifier, True):
                advice, warning = _find_advice_for_identifier(
                    entries, identifier, True)
                results.append(
                    BadIdentifier(identifier, line_number, advice, warning))

    return results


def main():
    for path in sys.stdin.read().splitlines():
        try:
            with open(path, 'r') as f:
                contents = f.read()
                disallowed_identifiers = check(
                    path,
                    [(i + 1, l) for i, l in enumerate(contents.splitlines())])
                if disallowed_identifiers:
                    print('%s uses disallowed identifiers:' % path)
                    for i in disallowed_identifiers:
                        print(i.line, i.identifier, i.advice)
        except (IOError, UnicodeDecodeError) as e:
            print('could not open %s: %s' % (path, e))


if __name__ == '__main__':
    sys.exit(main())
