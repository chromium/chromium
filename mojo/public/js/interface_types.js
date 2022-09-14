// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  // Constants ----------------------------------------------------------------
  var kInterfaceIdNamespaceMask = 0x80000000;
  var kPrimaryInterfaceId = 0x00000000;
  var kInvalidInterfaceId = 0xFFFFFFFF;

  // ---------------------------------------------------------------------------

  function InterfacePtrInfo(handle, version) {
    this.handle = handle;
    this.version = version;
  }

  InterfacePtrInfo.prototype.isValid = function() {
    return this.handle instanceof MojoHandle;
  };

  InterfacePtrInfo.prototype.close = function() {
    if (!this.isValid())
      return;

    this.handle.close();
    this.handle = null;
    this.version = 0;
  };

  function AssociatedInterfacePtrInfo(interfaceEndpointHandle, version) {
    this.interfaceEndpointHandle = interfaceEndpointHandle;
    this.version = version;
  }

  AssociatedInterfacePtrInfo.prototype.isValid = function() {
    return this.interfaceEndpointHandle.isValid();
  };

  // ---------------------------------------------------------------------------

  function InterfaceRequest(handle) {
    this.handle = handle;
  }

  InterfaceRequest.prototype.isValid = function() {
    return this.handle instanceof MojoHandle;
  };

  InterfaceRequest.prototype.close = function() {
    if (!this.isValid())
      return;

    this.handle.close();
    this.handle = null;
  };

  function AssociatedInterfaceRequest(interfaceEndpointHandle) {
    this.interfaceEndpointHandle = interfaceEndpointHandle;
  }

  AssociatedInterfaceRequest.prototype.isValid = function() {
    return this.interfaceEndpointHandle.isValid();
  };

  AssociatedInterfaceRequest.prototype.resetWithReason = function(reason) {
    this.interfaceEndpointHandle.reset(reason);
  };

  function isPrimaryInterfaceId(interfaceId) {
    return interfaceId === kPrimaryInterfaceId;
  }

  function isValidInterfaceId(interfaceId) {
    return interfaceId !== kInvalidInterfaceId;
  }

  function hasInterfaceIdNamespaceBitSet(interfaceId) {
    if (interfaceId >= 2 * kInterfaceIdNamespaceMask) {
      throw new Error("Interface ID should be a 32-bit unsigned integer.");
    }
    return interfaceId >= kInterfaceIdNamespaceMask;
  }

  mojo.InterfacePtrInfo = InterfacePtrInfo;
  mojo.InterfaceRequest = InterfaceRequest;
  mojo.AssociatedInterfacePtrInfo = AssociatedInterfacePtrInfo;
  mojo.AssociatedInterfaceRequest = AssociatedInterfaceRequest;
  internal.isPrimaryInterfaceId = isPrimaryInterfaceId;
  internal.isValidInterfaceId = isValidInterfaceId;
  internal.hasInterfaceIdNamespaceBitSet = hasInterfaceIdNamespaceBitSet;
  internal.kInvalidInterfaceId = kInvalidInterfaceId;
  internal.kPrimaryInterfaceId = kPrimaryInterfaceId;
  internal.kInterfaceIdNamespaceMask = kInterfaceIdNamespaceMask;
})();
