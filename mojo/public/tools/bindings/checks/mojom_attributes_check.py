# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validate mojo attributes are allowed in Chrome before generation."""

import mojom.generate.check as check
import mojom.generate.module as module

_COMMON_ATTRIBUTES = {
    'EnableIf',
    'EnableIfNot',
}

# For struct, union & parameter lists.
_COMMON_FIELD_ATTRIBUTES = _COMMON_ATTRIBUTES | {
    'MinVersion',
    'RenamedFrom',
}

# Note: `Default`` goes on the default _value_, not on the enum.
# Note: [Stable] without [Extensible] is not allowed.
_ENUM_ATTRIBUTES = _COMMON_ATTRIBUTES | {
    'Extensible',
    'Native',
    'Stable',
    'RenamedFrom',
    'Uuid',
}

# TODO(crbug.com/40192185) MinVersion is not needed for EnumVal.
_ENUMVAL_ATTRIBUTES = _COMMON_ATTRIBUTES | {
    'Default',
    'MinVersion',
}

_INTERFACE_ATTRIBUTES = _COMMON_ATTRIBUTES | {
    'DispatchDebugAlias',
    'RenamedFrom',
    'RequireContext',
    'RuntimeFeature',
    'ServiceSandbox',
    'Stable',
    'Uuid',
}

_METHOD_ATTRIBUTES = _COMMON_ATTRIBUTES | {
    'AllowedContext',
    'EstimateSize',
    'MinVersion',
    'NoInterrupt',
    'RuntimeFeature',
    'SupportsUrgent',
    'Sync',
    'UnlimitedSize',
}

_MODULE_ATTRIBUTES = _COMMON_ATTRIBUTES | {
    'JavaConstantsClassName',
    'JavaPackage',
}

_PARAMETER_ATTRIBUTES = _COMMON_FIELD_ATTRIBUTES

_STRUCT_ATTRIBUTES = _COMMON_ATTRIBUTES | {
    'CustomSerializer',
    'JavaClassName',
    'Native',
    'Stable',
    'RenamedFrom',
    'Uuid',
}

_STRUCT_FIELD_ATTRIBUTES = _COMMON_FIELD_ATTRIBUTES

_UNION_ATTRIBUTES = _COMMON_ATTRIBUTES | {
    'Extensible',
    'Stable',
    'RenamedFrom',
    'Uuid',
}

_UNION_FIELD_ATTRIBUTES = _COMMON_FIELD_ATTRIBUTES | {
    'Default',
}

# TODO(crbug.com/40758130) empty this set and remove the allowlist.
_STABLE_ONLY_ALLOWLISTED_ENUMS = {
    'crosapi.mojom.OptionalBool',
    'crosapi.mojom.TriState',
}

# TODO(crbug.com/393179188): Remove this allowlist. Do not add new entries here.
_NATIVE_ALLOWLIST = {
    'cc.mojom.BrowserControlsParams',
    'cc.mojom.BrowserControlsState',
    'cc.mojom.OverscrollBehavior',
    'cc.mojom.TouchAction',
    'chrome.mojom.FaviconUsageDataList',
    'chrome.mojom.ImportedBookmarkEntry',
    'chrome.mojom.ImporterAutofillFormDataEntry',
    'chrome.mojom.ImporterIE7PasswordInfo',
    'chrome.mojom.ImporterURLRow',
    'chrome.mojom.ImportItem',
    'chrome.mojom.SafeArchiveAnalyzerResults',
    'chrome.mojom.SearchEngineInfo',
    'chrome.mojom.SourceProfile',
    'content.mojom.DropData',
    'content.mojom.NavigationGesture',
    'content.mojom.NetworkConnectionType',
    'content.mojom.PageState',
    'content.mojom.PageTransition',
    'content.mojom.ScrollbarButtonsPlacement',
    'content.mojom.ScrollerStyle',
    'content.mojom.SystemThemeColor',
    'content.mojom.WebPluginInfo',
    'gpu.mojom.Capabilities',
    'gpu.mojom.ContextLostReason',
    'gpu.mojom.Error',
    'gpu.mojom.GLCapabilities',
    'gpu.mojom.SwapBuffersCompleteParams',
    'media.mojom.AudioCodec',
    'media.mojom.AudioCodecProfile',
    'media.mojom.AudioDecoderType',
    'media.mojom.AudioParameters',
    'media.mojom.BufferingState',
    'media.mojom.BufferingStateChangeReason',
    'media.mojom.CdmConfig',
    'media.mojom.CdmMessageType',
    'media.mojom.CdmSessionType',
    'media.mojom.ChannelLayout',
    'media.mojom.EmeInitDataType',
    'media.mojom.EncryptionScheme',
    'media.mojom.Exception',
    'media.mojom.FullscreenVideoStatus',
    'media.mojom.HdcpVersion',
    'media.mojom.KeyType',
    'media.mojom.MatrixID',
    'media.mojom.MediaContainerName',
    'media.mojom.MediaContentType',
    'media.mojom.MediaLogRecord',
    'media.mojom.MediaStatusState',
    'media.mojom.OutputDeviceStatus',
    'media.mojom.OverlayInfo',
    'media.mojom.PrimaryID',
    'media.mojom.RangeID',
    'media.mojom.SampleFormat',
    'media.mojom.Status',
    'media.mojom.StreamType',
    'media.mojom.SubsampleEntry',
    'media.mojom.TransferID',
    'media.mojom.Type',
    'media.mojom.VideoCodec',
    'media.mojom.VideoCodecProfile',
    'media.mojom.VideoDecoderType',
    'media.mojom.VideoPixelFormat',
    'media.mojom.WaitingReason',
    'media.mojom.WatchTimeKey',
    'mojo.test.NativeEnum',
    'mojo.test.data_view.TestNativeStruct',
    'mojo.test.PickledEnum',
    'mojo.test.PickledStruct',
    'mojo.test.TestNativeStructMojom',
    'mojo.test.TestNativeStructWithAttachmentsMojom',
    'mojo.test.UnmappedNativeStruct',
    'nacl.mojom.NaClErrorCode',
    'network.mojom.AuthCredentials',
    'network.mojom.CertVerifyResult',
    'network.mojom.ConnectionInfo',
    'network.mojom.CTPolicyCompliance',
    'network.mojom.EffectiveConnectionType',
    'network.mojom.HttpResponseHeaders',
    'network.mojom.P2PHostAndIPEndPoint',
    'network.mojom.P2PPacketInfo',
    'network.mojom.P2PPortRange',
    'network.mojom.P2PSendPacketMetrics',
    'network.mojom.P2PSocketOption',
    'network.mojom.P2PSocketType',
    'network.mojom.SSLInfo',
    'network.mojom.URLRequestRedirectInfo',
    'network.mojom.X509Certificate',
    'search.mojom.InstantMostVisitedInfo',
    'search.mojom.NTPLoggingEventType',
    'search.mojom.NtpTheme',
    'search.mojom.NTPTileImpression',
    'search.mojom.OmniboxFocusChangeReason',
    'search.mojom.OmniboxFocusState',
    'ui.mojom.EventPointerType',
    'ui.mojom.ScrollGranularity',
}


class Check(check.Check):
  def __init__(self, *args, **kwargs):
    super(Check, self).__init__(*args, **kwargs)

  def _Respell(self, allowed, attribute):
    for a in allowed:
      if a.lower() == attribute.lower():
        return f" - Did you mean: {a}?"
    return ""

  def _CheckAttributes(self, context, allowed, attributes):
    if not attributes:
      return
    for attribute in attributes:
      if not attribute in allowed:
        # Is there a close misspelling?
        hint = self._Respell(allowed, attribute)
        raise check.CheckException(
            self.module,
            f"attribute {attribute} not allowed on {context}{hint}")

  def _CheckEnumAttributes(self, enum):
    if enum.attributes:
      self._CheckAttributes("enum", _ENUM_ATTRIBUTES, enum.attributes)
      full_name = f"{self.module.mojom_namespace}.{enum.mojom_name}"
      if 'Stable' in enum.attributes and not 'Extensible' in enum.attributes:
        if full_name not in _STABLE_ONLY_ALLOWLISTED_ENUMS:
          raise check.CheckException(
              self.module,
              f"[Extensible] required on [Stable] enum {full_name}")
      if 'Native' in enum.attributes and full_name not in _NATIVE_ALLOWLIST:
        raise check.CheckException(
            self.module, f"[Native] is not allowed on {full_name}; "
            "no new uses should be introduced")
    for enumval in enum.fields:
      self._CheckAttributes("enum value", _ENUMVAL_ATTRIBUTES,
                            enumval.attributes)

  def _CheckInterfaceAttributes(self, interface):
    self._CheckAttributes("interface", _INTERFACE_ATTRIBUTES,
                          interface.attributes)
    for method in interface.methods:
      self._CheckAttributes("method", _METHOD_ATTRIBUTES, method.attributes)
      for param in method.parameters:
        self._CheckAttributes("parameter", _PARAMETER_ATTRIBUTES,
                              param.attributes)
      if method.response_parameters:
        for param in method.response_parameters:
          self._CheckAttributes("parameter", _PARAMETER_ATTRIBUTES,
                                param.attributes)
    for enum in interface.enums:
      self._CheckEnumAttributes(enum)

  def _CheckModuleAttributes(self):
    self._CheckAttributes("module", _MODULE_ATTRIBUTES, self.module.attributes)

  def _CheckStructAttributes(self, struct):
    self._CheckAttributes("struct", _STRUCT_ATTRIBUTES, struct.attributes)
    if struct.attributes and 'Native' in struct.attributes:
      full_name = f"{self.module.mojom_namespace}.{struct.mojom_name}"
      if full_name not in _NATIVE_ALLOWLIST:
        raise check.CheckException(
            self.module, f"[Native] is not allowed on {full_name}; "
            "no new uses should be introduced")
    for field in struct.fields:
      self._CheckAttributes("struct field", _STRUCT_FIELD_ATTRIBUTES,
                            field.attributes)
    for enum in struct.enums:
      self._CheckEnumAttributes(enum)

  def _CheckUnionAttributes(self, union):
    self._CheckAttributes("union", _UNION_ATTRIBUTES, union.attributes)
    for field in union.fields:
      self._CheckAttributes("union field", _UNION_FIELD_ATTRIBUTES,
                            field.attributes)

  def CheckModule(self):
    """Note that duplicate attributes are forbidden at the parse phase.
    We also do not need to look at the types of any parameters, as they will be
    checked where they are defined. Consts do not have attributes so can be
    skipped."""
    self._CheckModuleAttributes()
    for interface in self.module.interfaces:
      self._CheckInterfaceAttributes(interface)
    for enum in self.module.enums:
      self._CheckEnumAttributes(enum)
    for struct in self.module.structs:
      self._CheckStructAttributes(struct)
    for union in self.module.unions:
      self._CheckUnionAttributes(union)
