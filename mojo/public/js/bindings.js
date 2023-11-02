// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  // ---------------------------------------------------------------------------

  // |output| could be an interface pointer, InterfacePtrInfo or
  // AssociatedInterfacePtrInfo.
  function makeRequest(output) {
    if (output instanceof mojo.AssociatedInterfacePtrInfo) {
      var {handle0, handle1} = internal.createPairPendingAssociation();
      output.interfaceEndpointHandle = handle0;
      output.version = 0;

      return new mojo.AssociatedInterfaceRequest(handle1);
    }

    if (output instanceof mojo.InterfacePtrInfo) {
      var pipe = Mojo.createMessagePipe();
      output.handle = pipe.handle0;
      output.version = 0;

      return new mojo.InterfaceRequest(pipe.handle1);
    }

    var pipe = Mojo.createMessagePipe();
    output.ptr.bind(new mojo.InterfacePtrInfo(pipe.handle0, 0));
    return new mojo.InterfaceRequest(pipe.handle1);
  }

  // ---------------------------------------------------------------------------

  // Operations used to setup/configure an interface pointer. Exposed as the
  // |ptr| field of generated interface pointer classes.
  // |ptrInfoOrHandle| could be omitted and passed into bind() later.
  function InterfacePtrController(interfaceType, ptrInfoOrHandle) {
    this.version = 0;

    this.interfaceType_ = interfaceType;
    this.router_ = null;
    this.interfaceEndpointClient_ = null;
    this.proxy_ = null;

    // |router_| and |interfaceEndpointClient_| are lazily initialized.
    // |handle_| is valid between bind() and
    // the initialization of |router_| and |interfaceEndpointClient_|.
    this.handle_ = null;

    if (ptrInfoOrHandle)
      this.bind(ptrInfoOrHandle);
  }

  InterfacePtrController.prototype.bind = function(ptrInfoOrHandle) {
    this.reset();

    if (ptrInfoOrHandle instanceof mojo.InterfacePtrInfo) {
      this.version = ptrInfoOrHandle.version;
      this.handle_ = ptrInfoOrHandle.handle;
    } else {
      this.handle_ = ptrInfoOrHandle;
    }
  };

  InterfacePtrController.prototype.isBound = function() {
    return this.interfaceEndpointClient_ !== null || this.handle_ !== null;
  };

  // Although users could just discard the object, reset() closes the pipe
  // immediately.
  InterfacePtrController.prototype.reset = function() {
    this.version = 0;
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }
    if (this.router_) {
      this.router_.close();
      this.router_ = null;

      this.proxy_ = null;
    }
    if (this.handle_) {
      this.handle_.close();
      this.handle_ = null;
    }
  };

  InterfacePtrController.prototype.resetWithReason = function(reason) {
    if (this.isBound()) {
      this.configureProxyIfNecessary_();
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.reset();
  };

  InterfacePtrController.prototype.setConnectionErrorHandler = function(
      callback) {
    if (!this.isBound())
      throw new Error("Cannot set connection error handler if not bound.");

    this.configureProxyIfNecessary_();
    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  InterfacePtrController.prototype.passInterface = function() {
    var result;
    if (this.router_) {
      // TODO(yzshen): Fix Router interface to support extracting handle.
      result = new mojo.InterfacePtrInfo(
          this.router_.connector_.handle_, this.version);
      this.router_.connector_.handle_ = null;
    } else {
      // This also handles the case when this object is not bound.
      result = new mojo.InterfacePtrInfo(this.handle_, this.version);
      this.handle_ = null;
    }

    this.reset();
    return result;
  };

  InterfacePtrController.prototype.getProxy = function() {
    this.configureProxyIfNecessary_();
    return this.proxy_;
  };

  InterfacePtrController.prototype.configureProxyIfNecessary_ = function() {
    if (!this.handle_)
      return;

    this.router_ = new internal.Router(this.handle_, true);
    this.handle_ = null;

    this.interfaceEndpointClient_ = new internal.InterfaceEndpointClient(
        this.router_.createLocalEndpointHandle(internal.kPrimaryInterfaceId));

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateResponse]);
    this.proxy_ = new this.interfaceType_.proxyClass(
        this.interfaceEndpointClient_);
  };

  InterfacePtrController.prototype.queryVersion = function() {
    function onQueryVersion(version) {
      this.version = version;
      return version;
    }

    this.configureProxyIfNecessary_();
    return this.interfaceEndpointClient_.queryVersion().then(
      onQueryVersion.bind(this));
  };

  InterfacePtrController.prototype.requireVersion = function(version) {
    this.configureProxyIfNecessary_();

    if (this.version >= version) {
      return;
    }
    this.version = version;
    this.interfaceEndpointClient_.requireVersion(version);
  };

  // ---------------------------------------------------------------------------

  // |request| could be omitted and passed into bind() later.
  //
  // Example:
  //
  //    // FooImpl implements mojom.Foo.
  //    function FooImpl() { ... }
  //    FooImpl.prototype.fooMethod1 = function() { ... }
  //    FooImpl.prototype.fooMethod2 = function() { ... }
  //
  //    var fooPtr = new mojom.FooPtr();
  //    var request = makeRequest(fooPtr);
  //    var binding = new Binding(mojom.Foo, new FooImpl(), request);
  //    fooPtr.fooMethod1();
  function Binding(interfaceType, impl, requestOrHandle) {
    this.interfaceType_ = interfaceType;
    this.impl_ = impl;
    this.router_ = null;
    this.interfaceEndpointClient_ = null;
    this.stub_ = null;

    if (requestOrHandle)
      this.bind(requestOrHandle);
  }

  Binding.prototype.isBound = function() {
    return this.router_ !== null;
  };

  Binding.prototype.createInterfacePtrAndBind = function() {
    var ptr = new this.interfaceType_.ptrClass();
    // TODO(yzshen): Set the version of the interface pointer.
    this.bind(makeRequest(ptr));
    return ptr;
  };

  Binding.prototype.bind = function(requestOrHandle) {
    this.close();

    var handle = requestOrHandle instanceof mojo.InterfaceRequest ?
        requestOrHandle.handle : requestOrHandle;
    if (!(handle instanceof MojoHandle))
      return;

    this.router_ = new internal.Router(handle);

    this.stub_ = new this.interfaceType_.stubClass(this.impl_);
    this.interfaceEndpointClient_ = new internal.InterfaceEndpointClient(
        this.router_.createLocalEndpointHandle(internal.kPrimaryInterfaceId),
        this.stub_, this.interfaceType_.kVersion);

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateRequest]);
  };

  Binding.prototype.close = function() {
    if (!this.isBound())
      return;

    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }

    this.router_.close();
    this.router_ = null;
    this.stub_ = null;
  };

  Binding.prototype.closeWithReason = function(reason) {
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.close();
  };

  Binding.prototype.setConnectionErrorHandler = function(callback) {
    if (!this.isBound()) {
      throw new Error("Cannot set connection error handler if not bound.");
    }
    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  Binding.prototype.unbind = function() {
    if (!this.isBound())
      return new mojo.InterfaceRequest(null);

    var result = new mojo.InterfaceRequest(this.router_.connector_.handle_);
    this.router_.connector_.handle_ = null;
    this.close();
    return result;
  };

  // ---------------------------------------------------------------------------

  function BindingSetEntry(bindingSet, interfaceType, bindingType, impl,
      requestOrHandle, bindingId) {
    this.bindingSet_ = bindingSet;
    this.bindingId_ = bindingId;
    this.binding_ = new bindingType(interfaceType, impl,
        requestOrHandle);

    this.binding_.setConnectionErrorHandler(function(reason) {
      this.bindingSet_.onConnectionError(bindingId, reason);
    }.bind(this));
  }

  BindingSetEntry.prototype.close = function() {
    this.binding_.close();
  };

  function BindingSet(interfaceType) {
    this.interfaceType_ = interfaceType;
    this.nextBindingId_ = 0;
    this.bindings_ = new Map();
    this.errorHandler_ = null;
    this.bindingType_ = Binding;
  }

  BindingSet.prototype.isEmpty = function() {
    return this.bindings_.size == 0;
  };

  BindingSet.prototype.addBinding = function(impl, requestOrHandle) {
    this.bindings_.set(
        this.nextBindingId_,
        new BindingSetEntry(this, this.interfaceType_, this.bindingType_, impl,
            requestOrHandle, this.nextBindingId_));
    ++this.nextBindingId_;
  };

  BindingSet.prototype.closeAllBindings = function() {
    for (var entry of this.bindings_.values())
      entry.close();
    this.bindings_.clear();
  };

  BindingSet.prototype.setConnectionErrorHandler = function(callback) {
    this.errorHandler_ = callback;
  };

  BindingSet.prototype.onConnectionError = function(bindingId, reason) {
    this.bindings_.delete(bindingId);

    if (this.errorHandler_)
      this.errorHandler_(reason);
  };

  // ---------------------------------------------------------------------------

  // Operations used to setup/configure an associated interface pointer.
  // Exposed as |ptr| field of generated associated interface pointer classes.
  // |associatedPtrInfo| could be omitted and passed into bind() later.
  //
  // Example:
  //    // IntegerSenderImpl implements mojom.IntegerSender
  //    function IntegerSenderImpl() { ... }
  //    IntegerSenderImpl.prototype.echo = function() { ... }
  //
  //    // IntegerSenderConnectionImpl implements mojom.IntegerSenderConnection
  //    function IntegerSenderConnectionImpl() {
  //      this.senderBinding_ = null;
  //    }
  //    IntegerSenderConnectionImpl.prototype.getSender = function(
  //        associatedRequest) {
  //      this.senderBinding_ = new AssociatedBinding(mojom.IntegerSender,
  //          new IntegerSenderImpl(),
  //          associatedRequest);
  //    }
  //
  //    var integerSenderConnection = new mojom.IntegerSenderConnectionPtr();
  //    var integerSenderConnectionBinding = new Binding(
  //        mojom.IntegerSenderConnection,
  //        new IntegerSenderConnectionImpl(),
  //        mojo.makeRequest(integerSenderConnection));
  //
  //    // A locally-created associated interface pointer can only be used to
  //    // make calls when the corresponding associated request is sent over
  //    // another interface (either the primary interface or another
  //    // associated interface).
  //    var associatedInterfacePtrInfo = new AssociatedInterfacePtrInfo();
  //    var associatedRequest = makeRequest(interfacePtrInfo);
  //
  //    integerSenderConnection.getSender(associatedRequest);
  //
  //    // Create an associated interface and bind the associated handle.
  //    var integerSender = new mojom.AssociatedIntegerSenderPtr();
  //    integerSender.ptr.bind(associatedInterfacePtrInfo);
  //    integerSender.echo();

  function AssociatedInterfacePtrController(interfaceType, associatedPtrInfo) {
    this.version = 0;

    this.interfaceType_ = interfaceType;
    this.interfaceEndpointClient_ = null;
    this.proxy_ = null;

    if (associatedPtrInfo) {
      this.bind(associatedPtrInfo);
    }
  }

  AssociatedInterfacePtrController.prototype.bind = function(
      associatedPtrInfo) {
    this.reset();
    this.version = associatedPtrInfo.version;

    this.interfaceEndpointClient_ = new internal.InterfaceEndpointClient(
        associatedPtrInfo.interfaceEndpointHandle);

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateResponse]);
    this.proxy_ = new this.interfaceType_.proxyClass(
        this.interfaceEndpointClient_);
  };

  AssociatedInterfacePtrController.prototype.isBound = function() {
    return this.interfaceEndpointClient_ !== null;
  };

  AssociatedInterfacePtrController.prototype.reset = function() {
    this.version = 0;
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }
    if (this.proxy_) {
      this.proxy_ = null;
    }
  };

  AssociatedInterfacePtrController.prototype.resetWithReason = function(
      reason) {
    if (this.isBound()) {
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.reset();
  };

  // Indicates whether an error has been encountered. If true, method calls
  // on this interface will be dropped (and may already have been dropped).
  AssociatedInterfacePtrController.prototype.getEncounteredError = function() {
    return this.interfaceEndpointClient_ ?
        this.interfaceEndpointClient_.getEncounteredError() : false;
  };

  AssociatedInterfacePtrController.prototype.setConnectionErrorHandler =
      function(callback) {
    if (!this.isBound()) {
      throw new Error("Cannot set connection error handler if not bound.");
    }

    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  AssociatedInterfacePtrController.prototype.passInterface = function() {
    if (!this.isBound()) {
      return new mojo.AssociatedInterfacePtrInfo(null);
    }

    var result = new mojo.AssociatedInterfacePtrInfo(
        this.interfaceEndpointClient_.passHandle(), this.version);
    this.reset();
    return result;
  };

  AssociatedInterfacePtrController.prototype.getProxy = function() {
    return this.proxy_;
  };

  AssociatedInterfacePtrController.prototype.queryVersion = function() {
    function onQueryVersion(version) {
      this.version = version;
      return version;
    }

    return this.interfaceEndpointClient_.queryVersion().then(
      onQueryVersion.bind(this));
  };

  AssociatedInterfacePtrController.prototype.requireVersion = function(
      version) {
    if (this.version >= version) {
      return;
    }
    this.version = version;
    this.interfaceEndpointClient_.requireVersion(version);
  };

  // ---------------------------------------------------------------------------

  // |associatedInterfaceRequest| could be omitted and passed into bind()
  // later.
  function AssociatedBinding(interfaceType, impl, associatedInterfaceRequest) {
    this.interfaceType_ = interfaceType;
    this.impl_ = impl;
    this.interfaceEndpointClient_ = null;
    this.stub_ = null;

    if (associatedInterfaceRequest) {
      this.bind(associatedInterfaceRequest);
    }
  }

  AssociatedBinding.prototype.isBound = function() {
    return this.interfaceEndpointClient_ !== null;
  };

  AssociatedBinding.prototype.bind = function(associatedInterfaceRequest) {
    this.close();

    this.stub_ = new this.interfaceType_.stubClass(this.impl_);
    this.interfaceEndpointClient_ = new internal.InterfaceEndpointClient(
        associatedInterfaceRequest.interfaceEndpointHandle, this.stub_,
        this.interfaceType_.kVersion);

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateRequest]);
  };


  AssociatedBinding.prototype.close = function() {
    if (!this.isBound()) {
      return;
    }

    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }

    this.stub_ = null;
  };

  AssociatedBinding.prototype.closeWithReason = function(reason) {
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.close();
  };

  AssociatedBinding.prototype.setConnectionErrorHandler = function(callback) {
    if (!this.isBound()) {
      throw new Error("Cannot set connection error handler if not bound.");
    }
    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  AssociatedBinding.prototype.unbind = function() {
    if (!this.isBound()) {
      return new mojo.AssociatedInterfaceRequest(null);
    }

    var result = new mojo.AssociatedInterfaceRequest(
        this.interfaceEndpointClient_.passHandle());
    this.close();
    return result;
  };

  // ---------------------------------------------------------------------------

  function AssociatedBindingSet(interfaceType) {
    mojo.BindingSet.call(this, interfaceType);
    this.bindingType_ = AssociatedBinding;
  }

  AssociatedBindingSet.prototype = Object.create(BindingSet.prototype);
  AssociatedBindingSet.prototype.constructor = AssociatedBindingSet;

  mojo.makeRequest = makeRequest;
  mojo.AssociatedInterfacePtrController = AssociatedInterfacePtrController;
  mojo.AssociatedBinding = AssociatedBinding;
  mojo.AssociatedBindingSet = AssociatedBindingSet;
  mojo.Binding = Binding;
  mojo.BindingSet = BindingSet;
  mojo.InterfacePtrController = InterfacePtrController;
})();
