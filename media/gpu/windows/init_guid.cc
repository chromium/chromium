// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is defined here to ensure the D3D11, D3D9, and DXVA includes have their
// GUIDs intialized.
#define INITGUID
#include <d3d11.h>
#include <d3d9.h>
#include <dxva.h>
#undef INITGUID
