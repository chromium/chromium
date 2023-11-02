// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  var AssociationEvent = {
    // The interface has been associated with a message pipe.
    ASSOCIATED: 'associated',
    // The peer of this object has been closed before association.
    PEER_CLOSED_BEFORE_ASSOCIATION: 'peer_closed_before_association'
  };

  function State(interfaceId, associatedGroupController) {
    if (interfaceId === undefined) {
      interfaceId = internal.kInvalidInterfaceId;
    }

    this.interfaceId = interfaceId;
    this.associatedGroupController = associatedGroupController;
    this.pendingAssociation = false;
    this.disconnectReason = null;
    this.peerState_ = null;
    this.associationEventHandler_ = null;
  }

  State.prototype.initPendingState = function(peer) {
    this.pendingAssociation = true;
    this.peerState_ = peer;
  };

  State.prototype.isValid = function() {
    return this.pendingAssociation ||
        internal.isValidInterfaceId(this.interfaceId);
  };

  State.prototype.close = function(disconnectReason) {
    var cachedGroupController;
    var cachedPeerState;
    var cachedId = internal.kInvalidInterfaceId;

    if (!this.pendingAssociation) {
      if (internal.isValidInterfaceId(this.interfaceId)) {
        cachedGroupController = this.associatedGroupController;
        this.associatedGroupController = null;
        cachedId = this.interfaceId;
        this.interfaceId = internal.kInvalidInterfaceId;
      }
    } else {
      this.pendingAssociation = false;
      cachedPeerState = this.peerState_;
      this.peerState_ = null;
    }

    if (cachedGroupController) {
      cachedGroupController.closeEndpointHandle(cachedId,
          disconnectReason);
    } else if (cachedPeerState) {
      cachedPeerState.onPeerClosedBeforeAssociation(disconnectReason);
    }
  };

  State.prototype.runAssociationEventHandler = function(associationEvent) {
    if (this.associationEventHandler_) {
      var handler = this.associationEventHandler_;
      this.associationEventHandler_ = null;
      handler(associationEvent);
    }
  };

  State.prototype.setAssociationEventHandler = function(handler) {
    if (!this.pendingAssociation &&
        !internal.isValidInterfaceId(this.interfaceId)) {
      return;
    }

    if (!handler) {
      this.associationEventHandler_ = null;
      return;
    }

    this.associationEventHandler_ = handler;
    if (!this.pendingAssociation) {
      setTimeout(this.runAssociationEventHandler.bind(this,
          AssociationEvent.ASSOCIATED), 0);
    } else if (!this.peerState_) {
      setTimeout(this.runAssociationEventHandler.bind(this,
          AssociationEvent.PEER_CLOSED_BEFORE_ASSOCIATION), 0);
    }
  };

  State.prototype.notifyAssociation = function(interfaceId,
                                               peerGroupController) {
    var cachedPeerState = this.peerState_;
    this.peerState_ = null;

    this.pendingAssociation = false;

    if (cachedPeerState) {
      cachedPeerState.onAssociated(interfaceId, peerGroupController);
      return true;
    }
    return false;
  };

  State.prototype.onAssociated = function(interfaceId,
      associatedGroupController) {
    if (!this.pendingAssociation) {
      return;
    }

    this.pendingAssociation = false;
    this.peerState_ = null;
    this.interfaceId = interfaceId;
    this.associatedGroupController = associatedGroupController;
    this.runAssociationEventHandler(AssociationEvent.ASSOCIATED);
  };

  State.prototype.onPeerClosedBeforeAssociation = function(disconnectReason) {
    if (!this.pendingAssociation) {
      return;
    }

    this.peerState_ = null;
    this.disconnectReason = disconnectReason;

    this.runAssociationEventHandler(
        AssociationEvent.PEER_CLOSED_BEFORE_ASSOCIATION);
  };

  function createPairPendingAssociation() {
    var handle0 = new InterfaceEndpointHandle();
    var handle1 = new InterfaceEndpointHandle();
    handle0.state_.initPendingState(handle1.state_);
    handle1.state_.initPendingState(handle0.state_);
    return {handle0: handle0, handle1: handle1};
  }

  function InterfaceEndpointHandle(interfaceId, associatedGroupController) {
    this.state_ = new State(interfaceId, associatedGroupController);
  }

  InterfaceEndpointHandle.prototype.isValid = function() {
    return this.state_.isValid();
  };

  InterfaceEndpointHandle.prototype.pendingAssociation = function() {
    return this.state_.pendingAssociation;
  };

  InterfaceEndpointHandle.prototype.id = function() {
    return this.state_.interfaceId;
  };

  InterfaceEndpointHandle.prototype.groupController = function() {
    return this.state_.associatedGroupController;
  };

  InterfaceEndpointHandle.prototype.disconnectReason = function() {
    return this.state_.disconnectReason;
  };

  InterfaceEndpointHandle.prototype.setAssociationEventHandler = function(
      handler) {
    this.state_.setAssociationEventHandler(handler);
  };

  InterfaceEndpointHandle.prototype.notifyAssociation = function(interfaceId,
      peerGroupController) {
    return this.state_.notifyAssociation(interfaceId, peerGroupController);
  };

  InterfaceEndpointHandle.prototype.reset = function(reason) {
    this.state_.close(reason);
    this.state_ = new State();
  };

  internal.AssociationEvent = AssociationEvent;
  internal.InterfaceEndpointHandle = InterfaceEndpointHandle;
  internal.createPairPendingAssociation = createPairPendingAssociation;
})();
