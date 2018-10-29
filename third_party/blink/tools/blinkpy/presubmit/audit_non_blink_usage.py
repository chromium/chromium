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
            'base::AutoReset',
            'base::ElapsedTimer',
            'base::File',
            'base::FilePath',
            'base::GetUniqueIdForProcess',
            'base::Location',
            'base::MakeRefCounted',
            'base::Optional',
            'base::OptionalOrNullptr',
            'base::RefCountedData',
            'base::RunLoop',
            'base::CreateSequencedTaskRunnerWithTraits',
            'base::ReadOnlySharedMemoryMapping',
            'base::ReadOnlySharedMemoryRegion',
            'base::SequencedTaskRunner',
            'base::SingleThreadTaskRunner',
            'base::ScopedFD',
            'base::SupportsWeakPtr',
            'base::SysInfo',
            'base::ThreadChecker',
            'base::Time',
            'base::TimeDelta',
            'base::TimeTicks',
            'base::ThreadTicks',
            'base::UnguessableToken',
            'base::UnsafeSharedMemoryRegion',
            'base::WeakPtr',
            'base::WeakPtrFactory',
            'base::WritableSharedMemoryMapping',
            'base::in_place',
            'base::make_optional',
            'base::make_span',
            'base::nullopt',
            'base::sequence_manager::TaskTimeObserver',
            'base::size',
            'base::span',
            'logging::GetVlogLevel',

            # //base/bind_helpers.h.
            'base::DoNothing',

            # //base/callback.h is allowed, but you need to use WTF::Bind or
            # WTF::BindRepeating to create callbacks in Blink.
            'base::BarrierClosure',
            'base::OnceCallback',
            'base::OnceClosure',
            'base::RepeatingCallback',
            'base::RepeatingClosure',

            # //base/memory/ptr_util.h.
            'base::WrapUnique',

            # //base/allocator/partition_allocator/oom_callback.h.
            'base::SetPartitionAllocOomCallback',

            # //base/metrics/field_trial_params.h.
            'base::GetFieldTrialParamValueByFeature',
            'base::GetFieldTrialParamByFeatureAsBool',
            'base::GetFieldTrialParamByFeatureAsDouble',
            'base::GetFieldTrialParamByFeatureAsInt',

            # //base/numerics/safe_conversions.h.
            'base::as_signed',
            'base::as_unsigned',
            'base::checked_cast',
            'base::strict_cast',
            'base::saturated_cast',
            'base::SafeUnsignedAbs',
            'base::StrictNumeric',
            'base::MakeStrictNum',
            'base::IsValueInRangeForNumericType',
            'base::IsTypeInRangeForNumericType',
            'base::IsValueNegative',

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

            # Debugging helpers from //base/debug are allowed everywhere.
            'base::debug::.+',

            # Base atomic utilities
            'base::AtomicSequenceNumber',

            # Task traits
            'base::TaskTraits',
            'base::MayBlock',
            'base::TaskPriority',
            'base::TaskShutdownBehavior',
            'base::WithBaseSyncPrimitives',

            # Byte order
            'base::ByteSwap',
            'base::NetToHost(16|32|64)',
            'base::HostToNet(16|32|64)',

            # (Cryptographic) random number generation
            'base::RandUint64',
            'base::RandInt',
            'base::RandGenerator',
            'base::RandDouble',
            'base::RandBytes',

            # Feature list checking.
            'base::Feature.*',
            'base::FEATURE_.+',

            # PartitionAlloc
            'base::PartitionFree',

            # For MessageLoop::TaskObserver.
            'base::PendingTask',

            # cc painting types.
            'cc::PaintCanvas',
            'cc::PaintFlags',

            # Chromium geometry types.
            'gfx::Point',
            'gfx::Rect',
            'gfx::RectF',
            'gfx::Size',
            'gfx::SizeF',
            'gfx::Transform',
            'gfx::Vector2d',
            'gfx::Vector2dF',
            # Wrapper of SkRegion used in Chromium.
            'cc::Region',

            # A geometric set of TouchActions associated with areas, and only
            # depends on the geometry types above.
            'cc::TouchActionRegion',

            # Selection bounds.
            'cc::LayerSelection',
            'gfx::SelectionBound',

            # cc::Layers.
            'cc::Layer',
            'cc::PictureLayer',

            # cc::Layer helper data structs.
            'cc::ElementId',
            'cc::LayerPositionConstraint',
            'cc::LayerStickyPositionConstraint',
            'cc::OverscrollBehavior',
            'cc::Scrollbar',
            'cc::ScrollbarLayerInterface',
            'cc::ScrollbarOrientation',
            'cc::ScrollbarPart',

            # cc::Layer helper enums.
            'cc::HORIZONTAL',
            'cc::VERTICAL',
            'cc::THUMB',
            'cc::TICKMARKS',
            'cc::BrowserControlsState',
            'cc::EventListenerClass',
            'cc::EventListenerProperties',

            # Scrolling
            'cc::ScrollOffsetAnimationCurve',
            'cc::ScrollStateData',
            'gfx::RectToSkRect',
            'gfx::ScrollOffset',

            # Standalone utility libraries that only depend on //base
            'skia::.+',
            'url::.+',

            # Nested namespaces under the blink namespace
            'background_scheduler::.+',
            'canvas_heuristic_parameters::.+',
            'cssvalue::.+',
            'encoding::.+',
            'event_handling_util::.+',
            'event_util::.+',
            'media_constraints_impl::.+',
            'media_element_parser_helpers::.+',
            'network_utils::.+',
            'paint_filter_builder::.+',
            'root_scroller_util::.+',
            'scheduler::.+',
            'style_change_reason::.+',
            'svg_path_parser::.+',
            'touch_action_util::.+',
            'vector_math::.+',
            'web_core_test_support::.+',
            'xpath::.+',
            '[a-z_]+_names::.+',

            # Third-party libraries that don't depend on non-Blink Chrome code
            # are OK.
            'icu::.+',
            'testing::.+',  # googlemock / googletest
            'v8::.+',
            'v8_inspector::.+',

            # Inspector instrumentation and protocol
            'probe::.+',
            'protocol::.+',

            # Blink code shouldn't need to be qualified with the Blink namespace,
            # but there are exceptions.
            'blink::.+',
            # Assume that identifiers where the first qualifier is internal are
            # nested in the blink namespace.
            'internal::.+',

            # Network service.
            'network::.+',

            # Some test helpers live in the blink::test namespace.
            'test::.+',

            # Blink uses Mojo, so it needs mojo::Binding, mojo::InterfacePtr, et
            # cetera, as well as generated Mojo bindings.
            # Note that the Mojo callback helpers are explicitly forbidden:
            # Blink already has a signal for contexts being destroyed, and
            # other types of failures should be explicitly signalled.
            'mojo::(?!WrapCallback).+',
            'mojo_base::BigBuffer.*',
            '(?:.+::)?mojom::.+',
            "service_manager::BinderRegistry",
            # TODO(dcheng): Remove this once Connector isn't needed in Blink
            # anymore.
            'service_manager::Connector',
            'service_manager::InterfaceProvider',

            # STL containers such as std::string and std::vector are discouraged
            # but still needed for interop with WebKit/common. Note that other
            # STL types such as std::unique_ptr are encouraged.
            'std::.+',

            # Blink uses UKM for logging e.g. always-on leak detection (crbug/757374)
            'ukm::.+',
        ],
        'disallowed': [
            '.+',
            ('base::Bind(|Once|Repeating)',
             'Use WTF::Bind or WTF::BindRepeating.'),
        ],
    },
    {
        'paths': ['third_party/blink/renderer/bindings/'],
        'allowed': ['gin::.+'],
    },
    {
        'paths': ['third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.cc'],
        'allowed': [
            # For memory reduction histogram.
            'base::ProcessMetrics',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/clipboard'],
        'allowed': ['gfx::PNGCodec', 'net::EscapeForHTML'],
    },
    {
        'paths': ['third_party/blink/renderer/core/css'],
        'allowed': [
            # Internal implementation details for CSS.
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
        'paths': ['third_party/blink/renderer/core/fetch/data_consumer_handle_test_util.cc'],
        'allowed': [
            # The existing code already contains gin::IsolateHolder.
            'gin::IsolateHolder',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/paint'],
        'allowed': [
            # cc painting types.
            'cc::ContentLayerClient',
            'cc::DisplayItemList',
            'cc::DrawRecordOp',

            'paint_property_tree_printer::UpdateDebugNames',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/page/scrolling'],
        'allowed': [
            # cc scrollbar layer types.
            'cc::PaintedOverlayScrollbarLayer',
            'cc::PaintedScrollbarLayer',
            'cc::SolidColorScrollbarLayer',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/page'],
        'allowed': [
            'touch_adjustment::.+',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/inspector/inspector_memory_agent.cc'],
        'allowed': [
            'base::ModuleCache',
            'base::PoissonAllocationSampler',
            'base::SamplingHeapProfiler',
        ],
    },
    {
        'paths': ['third_party/blink/renderer/core/inspector/inspector_performance_agent.cc'],
        'allowed': [
            'base::subtle::TimeTicksNowIgnoringOverride',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/device_orientation/',
            'third_party/blink/renderer/modules/gamepad/',
            'third_party/blink/renderer/modules/sensor/',
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
            'gpu::gles2::GLES2Interface',
            'gpu::MailboxHolder',
            'display::Display',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/webgpu/',
        ],
        # The WebGPU Blink module needs access to the WebGPU control
        # command buffer interface.
        'allowed': [
            'gpu::webgpu::WebGPUInterface',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/platform/',
        ],
        # Suppress almost all checks on platform since code in this directory
        # is meant to be a bridge between Blink and non-Blink code. However,
        # base::RefCounted should still be explicitly blocked, since
        # WTF::RefCounted should be used instead.
        'allowed': ['(?!base::RefCounted).+'],
    },
    {
        'paths': [
            'third_party/blink/renderer/core/exported/',
            'third_party/blink/renderer/modules/exported/',
        ],
        'allowed': [
            'base::Time',
            'base::TimeTicks',
            'base::TimeDelta',
        ],
    },
    {
        'paths': [
            'third_party/blink/renderer/modules/animationworklet/',
        ],
        'allowed': [
            'cc::AnimationOptions',
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
            'third_party/blink/renderer/core/paint/fallback_theme.cc',
            'third_party/blink/renderer/core/paint/fallback_theme.h',
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
            'third_party/blink/renderer/modules/peerconnection',
            'third_party/blink/renderer/bindings/modules/v8/serialization',
        ],
        'allowed': [
            'cricket::.*',
            'rtc::.+',
            'webrtc::.+',
            'quic::.+',
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
            'absl::.+',
            'base::OnTaskRunnerDeleter',
            'sigslot::.+',
        ],
    }
]


def _precompile_config():
    """Turns the raw config into a config of compiled regex."""
    match_nothing_re = re.compile('.^')

    def compile_regexp(match_list):
        """Turns a match list into a compiled regexp.

        If match_list is None, a regexp that matches nothing is returned.
        """
        if match_list:
            return re.compile('(?:%s)$' % '|'.join(match_list))
        return match_nothing_re

    def compile_disallowed(disallowed_list):
        """Transforms the disallowed list to one with the regexps compiled."""
        if not disallowed_list:
            return match_nothing_re, []
        match_list = []
        advice_list = []
        for entry in disallowed_list:
            if isinstance(entry, tuple):
                match, advice = entry
                match_list.append(match)
                advice_list.append((compile_regexp(match), advice))
            else:
                # Just a string
                match_list.append(entry)
        return compile_regexp(match_list), advice_list

    compiled_config = []
    for raw_entry in _CONFIG:
        disallowed, advice = compile_disallowed(raw_entry.get('disallowed'))
        compiled_config.append({
            'paths': raw_entry['paths'],
            'allowed': compile_regexp(raw_entry.get('allowed')),
            'disallowed': disallowed,
            'advice': advice,
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
        if entry['allowed'].match(identifier):
            return True
        if entry['disallowed'].match(identifier):
            return False
    # Disallow by default.
    return False


def _find_advice_for_identifier(entries, identifier):
    advice_list = []
    for entry in entries:
        for matcher, advice in entry.get('advice', []):
            if matcher.match(identifier):
                advice_list.append(advice)
    return advice_list


class BadIdentifier(object):
    """Represents a single instance of a bad identifier."""
    def __init__(self, identifier, line, advice=None):
        self.identifier = identifier
        self.line = line
        self.advice = advice


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
    # Only check code. Ignore tests.
    # TODO(tkent): Remove 'Test' after the great mv.
    if (ext not in ('.cc', '.cpp', '.h', '.mm')
            or path.find('/testing/') >= 0
            or basename.endswith('Test')
            or basename.endswith('_test')
            or basename.endswith('_test_helpers')
            or basename.endswith('_unittest')):
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
                advice = _find_advice_for_identifier(entries, identifier)
                results.append(
                    BadIdentifier(identifier, line_number, advice))
    return results


def main():
    for path in sys.stdin.read().splitlines():
        try:
            with open(path, 'r') as f:
                contents = f.read()
                disallowed_identifiers = check(path, [
                    (i + 1, l) for i, l in
                    enumerate(contents.splitlines())])
                if disallowed_identifiers:
                    print '%s uses disallowed identifiers:' % path
                    for i in disallowed_identifiers:
                        print (i.line, i.identifier, i.advice)
        except IOError as e:
            print 'could not open %s: %s' % (path, e)


if __name__ == '__main__':
    sys.exit(main())
