# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .extended_attribute_descriptor import ExtendedAttributeDescriptor


def _build_supported_extended_attributes():
    E = ExtendedAttributeDescriptor
    T = ExtendedAttributeDescriptor.Target
    F = ExtendedAttributeDescriptor.Form

    # T_EXPOSURE is used at 'applicable_to' if an extended attribute changes
    # the exposure of the IDL construct.
    T_EXPOSURE = [
        T.ASYNC_ITERABLE, T.ATTRIBUTE, T.CALLBACK_INTERFACE, T.CONSTANT,
        T.CONSTRUCTOR, T.DICTIONARY_MEMBER, T.INTERFACE, T.ITERABLE,
        T.LEGACY_WINDOW_ALIAS, T.NAMESPACE, T.OPERATION
    ]
    # V_CALL_WITH is used at 'values' of [CallWith], [GetterCallWith], and
    # [SetterCallWith].
    V_CALL_WITH = [
        "Document", "ExecutionContext", "Isolate", "ScriptState", "ThisValue"
    ]

    # pv_readonly_attribute is used at 'post_validate' when an extended
    # attribute is applicable only to readonly attributes.
    def pv_readonly_attribute(assert_, target_object, ext_attr):
        assert_(target_object.is_readonly,
                "[{}] is applicable only to readonly attributes.",
                ext_attr.key)

    # Each entry must be in the form of:
    #
    #     E("name of an extended attribute",
    #       applicable_to=[T.TARGET1, T.TARGET2, ...],
    #       forms=[F.FORM1, F.FORM2, ...],
    #       values=["allowed value 1", "allowed value 2", ...],
    #       post_validate=func)
    #
    # where 'forms', 'values', and 'post_validate' are optional. If not
    # specified, it defaults to
    # 'forms=F.NO_ARGS' (doesn't take any argument/option),
    # 'values=None' (any argument/option is allowed), and
    # 'post_validate=None' (no additional validation).
    descriptors = [
        E("ActiveScriptWrappable", applicable_to=[T.INTERFACE]),
        E("Affects",
          applicable_to=[T.ATTRIBUTE, T.OPERATION],
          forms=F.IDENT,
          values=["Everything", "Nothing"]),
        E("AllowShared", applicable_to=[T.TYPE]),
        E("BufferSourceTypeNoSizeLimit", applicable_to=[T.TYPE]),
        E("CEReactions", applicable_to=[T.ATTRIBUTE, T.OPERATION]),
        E("CachedAccessor",
          applicable_to=[T.ATTRIBUTE],
          forms=F.IDENT,
          post_validate=pv_readonly_attribute),
        E("CachedAttribute",
          applicable_to=[T.ATTRIBUTE],
          forms=F.IDENT,
          post_validate=pv_readonly_attribute),
        E("CallWith",
          applicable_to=[T.ATTRIBUTE, T.CONSTRUCTOR, T.OPERATION],
          forms=[F.IDENT, F.IDENT_LIST],
          values=V_CALL_WITH),
        E("ConvertibleToObject", applicable_to=[T.DICTIONARY, T.TYPE]),
        E("CheckSecurity",
          applicable_to=[T.ATTRIBUTE, T.OPERATION],
          forms=[F.IDENT, F.IDENT_LIST],
          values=["ReturnValue"]),
        E("Clamp", applicable_to=[T.TYPE]),
        E("ContextEnabled", applicable_to=T_EXPOSURE, forms=F.IDENT),
        E("CrossOrigin",
          applicable_to=[T.ATTRIBUTE, T.OPERATION],
          forms=[F.NO_ARGS, F.IDENT, F.IDENT_LIST],
          values=["Getter", "Setter"]),
        E("CrossOriginIsolated", applicable_to=T_EXPOSURE),
        E("CrossOriginIsolatedOrRuntimeEnabled",
          applicable_to=T_EXPOSURE,
          forms=[F.IDENT]),
        E("DeprecateAs",
          applicable_to=[
              T.ATTRIBUTE, T.CONSTANT, T.CONSTRUCTOR, T.DICTIONARY_MEMBER,
              T.OPERATION
          ],
          forms=F.IDENT),
        E("EnforceRange", applicable_to=[T.TYPE]),
        E("Exposed",
          applicable_to=T_EXPOSURE,
          forms=[F.IDENT, F.IDENT_LIST, F.ARG_LIST]),
        E("GetterCallWith",
          applicable_to=[T.ATTRIBUTE],
          forms=[F.IDENT, F.IDENT_LIST],
          values=V_CALL_WITH),
        E("Global", applicable_to=[T.INTERFACE], forms=[F.IDENT,
                                                        F.IDENT_LIST]),
        E("HasAsyncIteratorReturnAlgorithm", applicable_to=[T.ASYNC_ITERABLE]),
        E("HighEntropy",
          applicable_to=[T.ATTRIBUTE, T.CONSTRUCTOR, T.OPERATION],
          forms=[F.NO_ARGS, F.IDENT],
          values=["Direct"]),
        E("HTMLConstructor", applicable_to=[T.CONSTRUCTOR, T.INTERFACE]),
        E("ImplementedAs",
          applicable_to=[
              T.ATTRIBUTE, T.CONSTRUCTOR, T.DICTIONARY, T.DICTIONARY_MEMBER,
              T.INTERFACE, T.NAMESPACE, T.OPERATION
          ],
          forms=F.IDENT),
        E("IDLTypeImplementedAsV8Promise", applicable_to=[T.TYPE]),
        E("InjectionMitigated", applicable_to=T_EXPOSURE),
        E("IsCodeLike", applicable_to=[T.INTERFACE]),
        E("IsolatedContext", applicable_to=T_EXPOSURE),
        E("LegacyFactoryFunction",
          applicable_to=[T.INTERFACE],
          forms=F.NAMED_ARG_LIST),
        E("LegacyFactoryFunction_CallWith",
          applicable_to=[T.INTERFACE],
          forms=[F.IDENT, F.IDENT_LIST],
          values=V_CALL_WITH),
        E("LegacyFactoryFunction_RaisesException",
          applicable_to=[T.INTERFACE]),
        E("LegacyLenientSetter", applicable_to=[T.ATTRIBUTE]),
        E("LegacyLenientThis", applicable_to=[T.ATTRIBUTE]),
        E("LegacyNoInterfaceObject", applicable_to=[T.INTERFACE]),
        E("LegacyNullToEmptyString", applicable_to=[T.TYPE]),
        E("LegacyOverrideBuiltIns", applicable_to=[T.INTERFACE]),
        E("LegacyTreatNonObjectAsNull", applicable_to=[T.CALLBACK_FUNCTION]),
        E("LegacyUnenumerableNamedProperties", applicable_to=[T.INTERFACE]),
        E("LegacyUnforgeable", applicable_to=[T.ATTRIBUTE, T.OPERATION]),
        E("LegacyWindowAlias", applicable_to=[T.INTERFACE], forms=F.IDENT),
        E("LegacyWindowAlias_Measure", applicable_to=[T.INTERFACE]),
        E("LegacyWindowAlias_MeasureAs",
          applicable_to=[T.INTERFACE],
          forms=F.IDENT),
        E("LegacyWindowAlias_RuntimeEnabled",
          applicable_to=[T.INTERFACE],
          forms=F.IDENT),
        E("LogActivity",
          applicable_to=[T.ATTRIBUTE, T.OPERATION],
          forms=[F.NO_ARGS, F.IDENT],
          values=["GetterOnly", "SetterOnly"]),
        E("LogAllWorlds", applicable_to=[T.OPERATION]),
        E("Measure",
          applicable_to=[
              T.ATTRIBUTE, T.CONSTANT, T.CONSTRUCTOR, T.DICTIONARY_MEMBER,
              T.LEGACY_WINDOW_ALIAS, T.OPERATION
          ]),
        E("MeasureAs",
          applicable_to=[
              T.ATTRIBUTE, T.CONSTANT, T.CONSTRUCTOR, T.DICTIONARY_MEMBER,
              T.LEGACY_WINDOW_ALIAS, T.OPERATION
          ],
          forms=F.IDENT),
        E("NewObject", applicable_to=[T.OPERATION]),
        E("NoAllocDirectCall", applicable_to=[T.OPERATION]),
        E("NodeWrapInOwnContext",
          applicable_to=[T.ATTRIBUTE, T.OPERATION, T.TYPE]),
        E("NotEnumerable", applicable_to=[T.ATTRIBUTE, T.OPERATION]),
        E("PassAsSpan", applicable_to=[T.TYPE]),
        E("PermissiveDictionaryConversion", applicable_to=[T.DICTIONARY]),
        E("PerWorldBindings", applicable_to=[T.ATTRIBUTE, T.OPERATION]),
        E("PromiseIDLTypeMismatch", applicable_to=[T.ATTRIBUTE, T.OPERATION]),
        E("PutForwards",
          applicable_to=[T.ATTRIBUTE],
          forms=F.IDENT,
          post_validate=pv_readonly_attribute),
        E("RaisesException",
          applicable_to=[T.ATTRIBUTE, T.CONSTRUCTOR, T.OPERATION],
          forms=[F.NO_ARGS, F.IDENT],
          values=["Getter", "Setter"]),
        E("Reflect", applicable_to=[T.ATTRIBUTE], forms=[F.NO_ARGS, F.IDENT]),
        E("ReflectEmpty", applicable_to=[T.ATTRIBUTE], forms=F.IDENT),
        E("ReflectInvalid", applicable_to=[T.ATTRIBUTE], forms=F.IDENT),
        E("ReflectMissing", applicable_to=[T.ATTRIBUTE], forms=F.IDENT),
        E("ReflectOnly", applicable_to=[T.ATTRIBUTE], forms=F.IDENT_LIST),
        E("Replaceable",
          applicable_to=[T.ATTRIBUTE],
          post_validate=pv_readonly_attribute),
        E("RuntimeEnabled", applicable_to=T_EXPOSURE, forms=F.IDENT),
        E("RuntimeCallStatsCounter",
          applicable_to=[T.ATTRIBUTE, T.OPERATION],
          forms=F.IDENT),
        E("SameObject", applicable_to=[T.ATTRIBUTE]),
        E("SaveSameObject", applicable_to=[T.ATTRIBUTE]),
        E("SecureContext", applicable_to=T_EXPOSURE),
        E("Serializable", applicable_to=[T.INTERFACE]),
        E("SetterCallWith",
          applicable_to=[T.ATTRIBUTE],
          forms=[F.IDENT, F.IDENT_LIST],
          values=V_CALL_WITH),
        E("StringContext",
          applicable_to=[T.TYPE],
          forms=F.IDENT,
          values=["TrustedHTML", "TrustedScript", "TrustedScriptURL"]),
        E("SupportsTaskAttribution", applicable_to=[T.CALLBACK_FUNCTION]),
        E("TargetOfExposed", applicable_to=[T.INTERFACE], forms=F.IDENT),
        E("Transferable", applicable_to=[T.INTERFACE]),
        E("URL", applicable_to=[T.ATTRIBUTE]),
        E("Unscopable", applicable_to=[T.ATTRIBUTE, T.OPERATION]),
    ]

    desc_map = dict()
    for desc in descriptors:
        desc_map[desc.name] = desc
    return desc_map


_SUPPORTED_EXTENDED_ATTRIBUTES = _build_supported_extended_attributes()


def validate(assert_, target_object, ext_attr):
    desc = _SUPPORTED_EXTENDED_ATTRIBUTES.get(ext_attr.key)
    if desc:
        desc.validate(assert_, target_object, ext_attr)
    else:
        assert_(False, "[{}] is an unknown IDL extended attribute.",
                ext_attr.key)
