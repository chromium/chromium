// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef LEGACY_IFACE
#define LEGACY_IFACE(iface_str, function_name)
#endif

LEGACY_IFACE(PPB_INPUT_EVENT_INTERFACE_1_0,
             ::ppapi::thunk::GetPPB_InputEvent_1_0_Thunk())
LEGACY_IFACE(PPB_INSTANCE_PRIVATE_INTERFACE_0_1,
             ::ppapi::thunk::GetPPB_Instance_Private_0_1_Thunk())
LEGACY_IFACE(PPB_CORE_INTERFACE_1_0, &core_interface)
LEGACY_IFACE(PPB_OPENGLES2_INTERFACE,
             ::ppapi::PPB_OpenGLES2_Shared::GetInterface())
LEGACY_IFACE(PPB_OPENGLES2_INSTANCEDARRAYS_INTERFACE,
             ::ppapi::PPB_OpenGLES2_Shared::GetInstancedArraysInterface())
LEGACY_IFACE(PPB_OPENGLES2_FRAMEBUFFERBLIT_INTERFACE,
             ::ppapi::PPB_OpenGLES2_Shared::GetFramebufferBlitInterface())
LEGACY_IFACE(
    PPB_OPENGLES2_FRAMEBUFFERMULTISAMPLE_INTERFACE,
    ::ppapi::PPB_OpenGLES2_Shared::GetFramebufferMultisampleInterface())
LEGACY_IFACE(PPB_OPENGLES2_CHROMIUMENABLEFEATURE_INTERFACE,
             ::ppapi::PPB_OpenGLES2_Shared::GetChromiumEnableFeatureInterface())
LEGACY_IFACE(PPB_OPENGLES2_CHROMIUMMAPSUB_INTERFACE,
             ::ppapi::PPB_OpenGLES2_Shared::GetChromiumMapSubInterface())
LEGACY_IFACE(PPB_OPENGLES2_CHROMIUMMAPSUB_DEV_INTERFACE_1_0,
             ::ppapi::PPB_OpenGLES2_Shared::GetChromiumMapSubInterface())
LEGACY_IFACE(PPB_OPENGLES2_QUERY_INTERFACE,
             ::ppapi::PPB_OpenGLES2_Shared::GetQueryInterface())
LEGACY_IFACE(PPB_OPENGLES2_DRAWBUFFERS_DEV_INTERFACE,
             ::ppapi::PPB_OpenGLES2_Shared::GetDrawBuffersInterface())
LEGACY_IFACE(PPB_PROXY_PRIVATE_INTERFACE, PPB_Proxy_Impl::GetInterface())
LEGACY_IFACE(PPB_VAR_DEPRECATED_INTERFACE,
             PPB_Var_Deprecated_Impl::GetVarDeprecatedInterface())
LEGACY_IFACE(PPB_VAR_INTERFACE_1_0,
             ::ppapi::PPB_Var_Shared::GetVarInterface1_0())
LEGACY_IFACE(PPB_VAR_INTERFACE_1_1,
             ::ppapi::PPB_Var_Shared::GetVarInterface1_1())
LEGACY_IFACE(PPB_VAR_INTERFACE_1_2,
             ::ppapi::PPB_Var_Shared::GetVarInterface1_2())
LEGACY_IFACE(PPB_VAR_ARRAY_BUFFER_INTERFACE_1_0,
             ::ppapi::PPB_Var_Shared::GetVarArrayBufferInterface1_0())


