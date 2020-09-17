#!/usr/bin/env vpython
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for checking for disallowed usage of non-Blink declarations.

The scanner assumes that usage of non-Blink code is always namespace qualified.
Identifiers in the global namespace are always ignored. For convenience, the
script can be run in standalone mode to check for existing violations.

Example command:

$ git ls-files third_party/blink \
    | python third_party/blink/tools/audit_non_blink_usage.py
"""

import os
import re
import sys

_CONFIG = [
    {
        'paths': ['third_party/blink/renderer/'],
        'allowed': [
            # TODO(dcheng): Should these be in a more specific config?
            'gfx::ColorSpace',
            'gfx::CubicBezier',
            'gfx::ICCProfile',
            'gfx::RadToDeg',

            # //base constructs that are allowed everywhere
            'base::AdoptRef',
            'base::ApplyMetadataToPastSamples',
            'base::AutoReset',
            'base::Contains',
            'base::CreateSequencedTaskRunner',
            'base::DefaultTickClock',
            'base::ElapsedTimer',
            'base::JobDelegate',
            'base::JobHandle',
            'base::PostJob',
            'base::File',
            'base::FilePath',
            'base::GetUniqueIdForProcess',
            "base::i18n::TextDirection",
            'base::Location',
            'base::MakeRefCounted',
            'base::Optional',
            'base::OptionalFromPtr',
            'base::OptionalOrNullptr',
            'base::PlatformThread',
            'base::PlatformThreadId',
            'base::RefCountedData',
            'base::RunLoop',
            'base::ReadOnlySharedMemoryMapping',
            'base::ReadOnlySharedMemoryRegion',
            'base::RepeatingTimer',
            'base::SequencedTaskRunner',
            'base::SingleThreadTaskRunner',
            'base::ScopedAllowBlocking',
            'base::ScopedFD',
            'base::ScopedClosureRunner',
            'base::SupportsWeakPtr',
            'base::SysInfo',
            'base::ThreadChecker',
            'base::TickClock',
            'base::Time',
            'base::TimeDelta',
            'base::TimeTicks',
            'base::ThreadTicks',
            'base::trace_event::MemoryAllocatorDump',
            'base::trace_event::MemoryDumpArgs',
            'base::trace_event::MemoryDumpManager',
            'base::trace_event::MemoryDumpProvider',
            'base::trace_event::ProcessMemoryDump',
            'base::UnguessableToken',
            'base::UnguessableTokenHash',
            'base::UnsafeSharedMemoryRegion',
            'base::WeakPtr',
            'base::WeakPtrFactory',
            'base::WritableSharedMemoryMapping',
            'base::as_bytes',
            'base::in_place',
            'base::make_optional',
            'base::make_span',
            'base::nullopt',
            'base::sequence_manager::TaskTimeObserver',
            'base::size',
            'base::span',
            'logging::GetVlogLevel',
            'util::PassKey',

            # //base/observer_list.h.
            'base::ObserverList',
            'base::CheckedObserver',

            # //base/bind_helpers.h.
            'base::DoNothing',

            # //base/callback.h is allowed, but you need to use WTF::Bind or
            # WTF::BindRepeating to create callbacks in Blink.
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
            'base::CancelableCallback',
            'base::CancelableClosure',

            # //base/mac/scoped_nsobject.h
            'base::scoped_nsobject',

            # //base/memory/scoped_policy.h
            'base::scoped_policy::RETAIN',

            # //base/memory/ptr_util.h.
            'base::WrapUnique',

            # //base/allocator/partition_allocator/oom_callback.h.
            'base::SetPartitionAllocOomCallback',

            # //base/containers/adapters.h
            'base::Reversed',

            # //base/metrics/histogram_functions.h
            'base::UmaHistogram.+',

            # //base/metrics/histogram.h
            'base::HistogramBase',
            'base::LinearHistogram',

            # //base/metrics/field_trial_params.h.
            'base::GetFieldTrialParamValueByFeature',
            'base::GetFieldTrialParamByFeatureAsBool',
            'base::GetFieldTrialParamByFeatureAsDouble',
            'base::GetFieldTrialParamByFeatureAsInt',

            # //base/numerics/safe_conversions.h.
            'base::as_signed',
            'base::as_unsigned',
            'base::checked_cast',
            'base::saturated_cast',
            'base::strict_cast',
            'base::ClampCeil',
            'base::ClampFloor',
            'base::IsTypeInRangeForNumericType',
            'base::IsValueInRangeForNumericType',
            'base::IsValueNegative',
            'base::MakeStrictNum',
            'base::ClampRound',
            'base::SafeUnsignedAbs',
            'base::StrictNumeric',

            # //base/strings/char_traits.h.
            'base::CharTraits',

            # //base/synchronization/waitable_event.h.
            'base::WaitableEvent',

            # //base/numerics/checked_math.h.
            'base::CheckedNumeric',
            'base::IsValidForType',
            'base::ValueOrDieForType',
            'base::ValueOrDefaultForType',
            'base::MakeCheckedNum',
            'base::CheckMax',
            'base::CheckMin',
            'base::CheckAdd',
            'base::CheckSub',
            'base::CheckMul',
            'base::CheckDiv',
            'base::CheckMod',
            'base::CheckLsh',
            'base::CheckRsh',
            'base::CheckAnd',
            'base::CheckOr',
            'base::CheckXor',

            # //base/numerics/clamped_math.h.
            'base::ClampAdd',
            'base::ClampSub',
            'base::MakeClampedNum',

            # //base/numerics/ranges.h.
            "base::ClampToRange",

            # //base/strings/strcat.h.
            'base::StrCat',

            # //base/template_util.h.
            'base::void_t',

            # Debugging helpers from //base/debug are allowed everywhere.
            'base::debug::.+',

            # Base atomic utilities
            'base::AtomicFlag',
            'base::AtomicSequenceNumber',

            # Task traits
            'base::TaskTraits',
            'base::MayBlock',
            'base::TaskPriority',
            'base::TaskShutdownBehavior',
            'base::WithBaseSyncPrimitives',
            'base::ThreadPolicy',
            'base::ThreadPool',

            # Byte order
            'base::ByteSwap',
            'base::ReadBigEndian',
            'base::NetToHost(16|32|64)',
            'base::HostToNet(16|32|64)',

            # (Cryptographic) random number generation
            'base::RandUint64',
            'base::RandInt',
            'base::RandGenerator',
            'base::RandDouble',
            'base::RandBytes',
            'base::RandBytesAsString',

            # Feature list checking.
            'base::Feature.*',
            'base::FEATURE_.+',
            "base::GetFieldTrial.*",
            'features::.+',

            # PartitionAlloc
            'base::PartitionFree',
            'base::PartitionAllocZeroFill',
            'base::PartitionAllocReturnNull',

            # For TaskObserver.
            'base::PendingTask',

            # Time
            'base::Clock',
            'base::DefaultClock',
            'base::DefaultTickClock',
            'base::TestMockTimeTaskRunner',
            'base::TickClock',

            # cc painting types.
            'cc::PaintCanvas',
            'cc::PaintFlags',
            'cc::PaintImage',
            'cc::PaintImageBuilder',
            'cc::PaintRecord',
            'cc::PaintShader',
            'cc::PaintWorkletInput',
            'cc::NodeId',
            'cc::NodeInfo',

            # Chromium geometry types.
            'gfx::Point',
            'gfx::PointF',
            'gfx::Point3F',
            'gfx::Quaternion',
            'gfx::Rect',
            'gfx::RectF',
            'gfx::RRectF',
            'gfx::ScaleToCeiledSize',
            'gfx::ScaleVector2d',
            'gfx::Size',
            'gfx::SizeF',
            'gfx::Transform',
            'gfx::Vector2d',
            'gfx::Vector2dF',

            # Chromium geometry operations.
            'gfx::ToFlooredPoint',

            # Range type.
            'gfx::Range',

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
            'cc::HORIZONTAL',
            'cc::VERTICAL',
            'cc::THUMB',
            'cc::TRACK_BUTTONS_TICKMARKS',
            'cc::BrowserControlsState',
            'cc::EventListenerClass',
            'cc::EventListenerProperties',

            # Animation
            'cc::AnimationHost',

            # UMA Enums
            'cc::PaintHoldingCommitTrigger',

            # Scrolling
            'cc::kManipulationInfoPinchZoom',
            'cc::kManipulationInfoPrecisionTouchPad',
            'cc::kManipulationInfoTouch',
            'cc::kManipulationInfoWheel',
            'cc::kPixelsPerLineStep',
            'cc::kMinFractionToStepWhenPaging',
            'cc::kPercentDeltaForDirectionalScroll',
            'cc::MainThreadScrollingReason',
            'cc::ManipulationInfo',
            'cc::ScrollSnapAlign',
            'cc::ScrollSnapType',
            'cc::ScrollOffsetAnimationCurve',
            'cc::ScrollStateData',
            'cc::ScrollUtils',
            'cc::SnapAlignment',
            'cc::SnapAreaData',
            'cc::SnapAxis',
            'cc::SnapContainerData',
            'cc::SnapFlingClient',
            'cc::SnapFlingController',
            'cc::SnapSelectionStrategy',
            'cc::SnapStrictness',
            'cc::TargetSnapAreaElementIds',
            'gfx::RectToSkRect',
            'gfx::RectToSkIRect',
            'gfx::ScrollOffset',
            'ui::ScrollGranularity',

            # base/util/type_safety/strong_alias.h
            'util::StrongAlias',

            # Standalone utility libraries that only depend on //base
            'skia::.+',
            'url::.+',

            # Nested namespaces under the blink namespace
            'bindings::.+',
            'canvas_heuristic_parameters::.+',
            'compositor_target_property::.+',
            'cors::.+',
            'css_parsing_utils::.+',
            'cssvalue::.+',
            'encoding::.+',
            'encoding_enum::.+',
            'event_handling_util::.+',
            'event_util::.+',
            'file_error::.+',
            'geometry_util::.+',
            'inspector_\\w+_event::.+',
            'inspector_async_task::.+',
            'inspector_set_layer_tree_id::.+',
            'inspector_tracing_started_in_frame::.+',
            'keywords::.+',
            'layered_api::.+',
            'layout_invalidation_reason::.+',
            'media_constraints_impl::.+',
            'media_element_parser_helpers::.+',
            'native_file_system_error::.+',
            'network_utils::.+',
            'origin_trials::.+',
            'paint_filter_builder::.+',
            'root_scroller_util::.+',
            'scheduler::.+',
            'scroll_customization::.+',
            'scroll_timeline_util::.+',
            'style_change_extra_data::.+',
            'style_change_reason::.+',
            'svg_path_parser::.+',
            'touch_action_util::.+',
            'trace_event::.+',
            'unicode::.+',
            'vector_math::.+',
            'web_core_test_support::.+',
            'worker_pool::.+',
            'xpath::.+',
            '[a-z_]+_names::.+',

            # Third-party libraries that don't depend on non-Blink Chrome code
            # are OK.
            'icu::.+',
            'testing::.+',  # googlemock / googletest
            'v8::.+',
            'v8_inspector::.+',
            'inspector_protocol_encoding::.+',

            # Inspector instrumentation and protocol
            'probe::.+',
            'protocol::.+',

            # Blink code shouldn't need to be qualified with the Blink namespace,
            # but there are exceptions.
            'blink::.+',
            # Assume that identifiers where the first qualifier is internal are
            # nested in the blink namespace.
            'internal::.+',

            # HTTP structured headers
            'net::structured_headers::.+',

            # HTTP status codes
            'net::HTTP_.+',

            # For ConnectionInfo enumeration
            'net::HttpResponseInfo',

            # Network service.
            'network::.+',

            # Used in network service types.
            'net::SiteForCookies',

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
            'mojo::(?!WrapCallback).+',
            'mojo_base::BigBuffer.*',
            '(?:.+::)?mojom::.+',
            'service_manager::InterfaceProvider',

            # STL containers such as std::string and std::vector are discouraged
            # but still needed for interop with WebKit/common. Note that other
            # STL types such as std::unique_ptr are encouraged.
            'std::.+',

            # UI Cursor
            'ui::Cursor',

            # UI Keyconverter
            'ui::DomCode',
            'ui::DomKey',
            'ui::KeycodeConverter',

            # Accessibility base types and the non-Blink enums they
            # depend on.
            'ui::AXEvent',
            'ui::AXEventIntent',
            'ui::AXMode',
            'ui::AXNodeData',
            'ui::IsDialog',
            'ui::IsContainerWithSelectableChildren',
            'ax::mojom::BoolAttribute',
            'ax::mojom::HasPopup',
            'ax::mojom::State',
            'ax::mojom::Restriction',

            # Blink uses UKM for logging e.g. always-on leak detection (crbug/757374)
            'ukm::.+',

            # Permit using crash keys inside Blink without jumping through
            # hoops.
            'crash_reporter::.*CrashKey.*',

            # Useful for platform-specific code.
            'base::mac::(CFToNSCast|NSToCFCast)',
            'base::mac::Is(AtMost|AtLeast)?OS.+',
            'base::(scoped_nsobject|ScopedCFTypeRef)',
        ],
        'disallowed': [
            ('base::Bind(|Once|Repeating)',
             'Use WTF::Bind or WTF::BindRepeating.'),
            ('std::(deque|map|multimap|set|vector|unordered_set|unordered_map)',
             'Use WTF containers like WTF::Deque, WTF::HashMap, WTF::HashSet or WTF::Vector instead of the banned std containers. '
             'However, it is fine to use std containers at the boundary layer between Blink and Chromium. '
             'If you are in this case, you can use --bypass-hooks option to avoid the presubmit check when uploading your CL.'
             ),
            # network::mojom::Foo is allowed to use as non-blink mojom type.
            ('(|::)(?!network::)(\w+::)?mojom::(?!blink).+',
             'Using non-blink mojom types, consider using "::mojom::blink::Foo" instead of "::mojom::Foo" unless you have clear reasons not to do so',
             'Warning'),
        ],
    },
    {
        'paths': ['third_party/blink/renderer/bindings/'],
        'allowed': ['gin::.+'],
    },
    {
        'paths':
        ['third_party/blink/renderer/bindings/core/v8/script_streamer.cc'],
        'allowed': [
            # For the script streaming to be able to block when reading from a
            # mojo datapipe.
            'base::ScopedAllowBaseSyncPrimitives',
            'base::ScopedBlockingCall',
            'base::BlockingType',
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
        'paths': ['third_party/blink/renderer/core/offscreencanvas'],
        'allowed': [
            # Flags to be used to set up sharedImage
            'gpu::SHARED_IMAGE_USAGE_DISPLAY',
            'gpu::SHARED_IMAGE_USAGE_SCANOUT',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.cc'
        ],
        'allowed': [
            'gpu::SHARED_IMAGE_USAGE_DISPLAY',
            'gpu::SHARED_IMAGE_USAGE_SCANOUT',
            'gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE',
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
            'cc::LayerTreeSettings',
            'cc::TaskGraphRunner',
            'gfx::DisplayColorSpaces',
            'ui::ImeTextSpan',
            'viz::FrameSinkId',
            'viz::LocalSurfaceIdAllocation',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/clipboard'],
        'allowed': ['net::EscapeForHTML'],
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
        'paths': ['third_party/blink/renderer/core/css/media_values.cc'],
        'allowed': [
            'color_space_utilities::GetColorSpaceGamut',
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
            'third_party/blink/renderer/core/fetch/data_consumer_handle_test_util.cc'
        ],
        'allowed': [
            # The existing code already contains gin::IsolateHolder.
            'gin::IsolateHolder',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/frame/visual_viewport.cc'],
        'allowed': [
            'cc::SolidColorScrollbarLayer',
        ],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/frame/web_frame_widget_base.cc'],
        'allowed': [
            'cc::InputHandlerScrollResult',
            'cc::SwapPromise',
            'viz::CompositorFrameMetadata',
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
        'paths':
        ['third_party/blink/renderer/core/fileapi/file_reader_loader.cc'],
        'allowed': [
            'net::ERR_FILE_NOT_FOUND',
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
        'paths': ['third_party/blink/renderer/core/page/scrolling'],
        'allowed': [
            'cc::ScrollbarLayerBase',
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
        'paths': ['third_party/blink/renderer/core/style/computed_style.h'],
        'allowed': [
            'css_longhand::.+',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/inspector/inspector_memory_agent.cc'
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
        'paths':
        ['third_party/blink/renderer/core/inspector/locale_controller.cc'],
        'allowed': [
            'base::i18n::SetICUDefaultLocale',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/inspector'],
        'allowed': [
            # Devtools binary protocol uses std::vector<uint8_t> for serialized
            # objects.
            'std::vector',
            # [C]h[R]ome [D]ev[T]ools [P]rotocol implementation support library
            # (see third_party/inspector_protocol/crdtp).
            'crdtp::.+',
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
        'paths': ['third_party/blink/renderer/core/workers/worker_thread.cc'],
        'allowed': [
            'base::ScopedAllowBaseSyncPrimitives',
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
            'third_party/blink/renderer/modules/vr/',
            'third_party/blink/renderer/modules/webgl/',
            'third_party/blink/renderer/modules/xr/',
        ],
        # The modules listed above need access to the following GL drawing and
        # display-related types.
        'allowed': [
            'base::MRUCache',
            'gl::GpuPreference',
            'gpu::gles2::GLES2Interface',
            'gpu::raster::RasterInterface',
            'gpu::Mailbox',
            'gpu::MailboxHolder',
            'display::Display',
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
            'third_party/blink/renderer/modules/webcodecs/',
        ],
        'allowed': [
            'gpu::kNullSurfaceHandle',
            'media::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/encryptedmedia/',
            'third_party/blink/renderer/modules/media/',
            'third_party/blink/renderer/modules/media_capabilities/',
            'third_party/blink/renderer/modules/video_rvfc/',
        ],
        'allowed': [
            'media::.+',
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
            'base::Unretained',
            'mojo::WrapCallbackWithDefaultInvokeIfNotRun',
            'base::ScopedPlatformFile',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/imagecapture/',
        ],
        'allowed': [
            'media::.+',
            'libyuv::.+',
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
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/mediarecorder/',
        ],
        'allowed': [
            'base::data',
            # TODO(crbug.com/960665): Remove base::queue once it is replaced with a WTF equivalent.
            'base::queue',
            'base::SharedMemory',
            'base::StringPiece',
            'base::ThreadTaskRunnerHandle',
            'media::.+',
            'libopus::.+',
            'libyuv::.+',
            'video_track_recorder::.+',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/mediastream/',
        ],
        'allowed': [
            'media::.+',
            'base::AutoLock',
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
            # gin::NamedPropertyInterceptor uses std::vector.
            'std::vector',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webcodecs/',
        ],
        'allowed': [
            'media::.+',
            'libyuv::.+',
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
        # The WebGPU Blink module needs access to the WebGPU control
        # command buffer interface.
        'allowed': [
            'gpu::webgpu::PowerPreference',
            'gpu::webgpu::WebGPUInterface',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webrtc/',
        ],
        'allowed': [
            'base::AutoLock',
            'base::Erase',
            'base::Lock',
            'base::StringPrintf',
            'media::.+',
            'rtc::scoped_refptr',
            'webrtc::AudioDeviceModule',
            'webrtc::AudioSourceInterface',
            'webrtc::AudioTransport',
            'webrtc::kAdmMaxDeviceNameSize',
            'webrtc::kAdmMaxGuidSize',
        ]
    },
    {
        'paths': [
            'third_party/blink/renderer/platform/',
        ],
        # Suppress almost all checks on platform since code in this directory is
        # meant to be a bridge between Blink and non-Blink code. However,
        # base::RefCounted should still be explicitly blocked, since
        # WTF::RefCounted should be used instead. base::RefCountedThreadSafe is
        # still needed for cross_thread_copier.h though.
        'allowed': ['base::RefCountedThreadSafe', '(?!base::RefCounted).+'],
    },
    {
        'paths': [
            'third_party/blink/renderer/platform/scheduler/common/single_thread_idle_task_runner.h',
        ],
        # base::RefCounted is prohibited in platform/ as defined above, but
        # SingleThreadIdleTaskRunner needs to be constructed before WTF and
        # PartitionAlloc are initialized, which forces us to use
        # base::RefCountedThreadSafe for it.
        'allowed': ['.+'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/exported/',
            'third_party/blink/renderer/core/input/',
        ],
        'allowed': [
            'ui::LatencyInfo',
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
            'third_party/blink/renderer/modules/webaudio/',
        ],
        'allowed': ['audio_utilities::.+'],
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
            'third_party/blink/renderer/core/paint/object_painter_base.cc',
            'third_party/blink/renderer/core/paint/theme_painter.cc',
        ],
        'allowed': ['ui::NativeTheme.*'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/layout/',
            'third_party/blink/renderer/core/paint/',
        ],
        'allowed': ['list_marker_text::.+'],
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
            'base::AutoLock',
            'base::AutoUnlock',
            'base::LazyInstance',
            'base::Lock',
            # TODO(crbug.com/787254): Remove base::BindOnce, base::Unretained,
            # base::Passed, base::Closure, base::CurrentThread and
            # base::RetainedRef.
            'base::Bind.*',
            'base::Closure',
            'base::MD5.*',
            'base::CurrentThread',
            'base::Passed',
            'base::PowerObserver',
            'base::RetainedRef',
            'base::StringPrintf',
            'base::Value',
            'base::Unretained',
            # TODO(crbug.com/787254): Replace base::Thread with the appropriate Blink class.
            'base::Thread',
            'base::WrapRefCounted',
            'cricket::.*',
            'jingle_glue::JingleThreadWrapper',
            # TODO(crbug.com/787254): Remove GURL usage.
            'GURL',
            'media::.+',
            'net::NetworkTrafficAnnotationTag',
            'net::DefineNetworkTrafficAnnotation',
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
        'paths': ['third_party/blink/renderer/modules/manifest/'],
        'allowed': ['net::ParseMimeTypeWithoutParameter'],
    },
    {
        'paths':
        ['third_party/blink/renderer/core/fetch/fetch_request_data.cc'],
        'allowed': ['net::RequestPriority'],
    },
    {
        'paths': ['third_party/blink/renderer/core/frame/local_frame_view.cc'],
        'allowed':
        ['cc::frame_viewer_instrumentation::IsTracingLayerTreeSnapshots'],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webaudio/offline_audio_worklet_thread.cc',
            'third_party/blink/renderer/modules/webaudio/realtime_audio_worklet_thread.cc',
            'third_party/blink/renderer/modules/webaudio/semi_realtime_audio_worklet_thread.cc',
        ],
        'allowed': ['base::ThreadPriority'],
    },
    {
        'paths': ['third_party/blink/renderer/core/frame/local_dom_window.cc'],
        'allowed': [
            'net::registry_controlled_domains::.+',
        ],
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
        compiled_config.append({
            'paths':
            raw_entry['paths'],
            'allowed':
            compile_regexp(raw_entry.get('allowed')),
            'disallowed':
            disallowed,
            'advice':
            advice,
        })
    return compiled_config


_COMPILED_CONFIG = _precompile_config()

# Attempt to match identifiers qualified with a namespace. Since parsing C++ in
# Python is hard, this regex assumes that namespace names only contain lowercase
# letters, numbers, and underscores, matching the Google C++ style guide. This
# is intended to minimize the number of matches where :: is used to qualify a
# name with a class or enum name.
#
# As a bit of a minor hack, this regex also hardcodes a check for GURL, since
# GURL isn't namespace qualified and wouldn't match otherwise.
_IDENTIFIER_WITH_NAMESPACE_RE = re.compile(
    r'\b(?:(?:[a-z_][a-z0-9_]*::)+[A-Za-z_][A-Za-z0-9_]*|GURL)\b')


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


def _check_entries_for_identifier(entries, identifier):
    """Check if an identifier is allowed"""
    for entry in entries:
        if entry['disallowed'].match(identifier):
            return False
        if entry['allowed'].match(identifier):
            return True
    # Disallow by default.
    return False


def _find_advice_for_identifier(entries, identifier):
    advice_list = []
    for entry in entries:
        for matcher, advice, warning in entry.get('advice', []):
            if matcher.match(identifier):
                advice_list.append(advice)
    return advice_list, warning


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
            or path.find('/tests/') >= 0 or basename.endswith('_test')
            or basename.endswith('_test_helpers')
            or basename.endswith('_unittest') or basename.endswith('_fuzzer')):
        return results
    entries = _find_matching_entries(path)
    if not entries:
        return
    for line_number, line in contents:
        idx = line.find('//')
        if idx >= 0:
            line = line[:idx]
        match = _IDENTIFIER_WITH_NAMESPACE_RE.search(line)
        if match:
            identifier = match.group(0)
            if not _check_entries_for_identifier(entries, identifier):
                advice, warning = _find_advice_for_identifier(
                    entries, identifier)
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
                    print '%s uses disallowed identifiers:' % path
                    for i in disallowed_identifiers:
                        print(i.line, i.identifier, i.advice)
        except IOError as e:
            print 'could not open %s: %s' % (path, e)


if __name__ == '__main__':
    sys.exit(main())
