// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Default transform functions, overridden by some test cases.
var transformSdp = function(sdp) { return sdp; };
var transformRemoteSdp = function(sdp) { return sdp; };
var onLocalDescriptionError = function(error) { failTest(error); };

// Provide functions to set the transform functions.
function setOfferSdpTransform(newTransform) {
  transformSdp = newTransform;
}
function setRemoteSdpTransform(newTransform) {
  transformRemoteSdp = newTransform;
}
function setOnLocalDescriptionError(newHandler) {
  onLocalDescriptionError = newHandler;
}

/**
 * Negotiate a call between two peer connections.
 *
 * Offer/Answer will be created and SDP candidates exchanged;
 * the functions verifiy the peer connections reach a steady
 * state.
 *
 * Offer and Answer SDP can be modified by setting one of
 * the transform functions above.
 *
 * Args:
 *   caller, callee: RTCPeerConnection instances.
 */
function negotiateBetween(caller, callee) {
  console.log("Negotiating call...");
  // Not stable = negotiation is ongoing. The behavior of re-negotiating while
  // a negotiation is ongoing is more or less undefined, so avoid this.
  if (caller.signalingState != 'stable' || callee.signalingState != 'stable')
    throw 'You can only negotiate when the connection is stable!';

  connectOnIceCandidate_(caller, callee);

  caller.createOffer(
    function (offer) {
      onOfferCreated_(offer, caller, callee);
    },
    function(error) {}
  );
}

/**
 * @private
 */
function onOfferCreated_(offer, caller, callee) {
  offer.sdp = transformSdp(offer.sdp);
  console.log('Offer:\n' + offer.sdp);
  caller.setLocalDescription(offer, function() {
    assertEquals('have-local-offer', caller.signalingState);
    receiveOffer_(offer.sdp, caller, callee);
  }, onLocalDescriptionError);
}

/**
 * @private
 */
function receiveOffer_(offerSdp, caller, callee) {
  console.log("Receiving offer...");
  offerSdp = transformRemoteSdp(offerSdp);

  var parsedOffer = new RTCSessionDescription({ type: 'offer',
                                                sdp: offerSdp });
  callee.setRemoteDescription(parsedOffer,
                              function() {
                                assertEquals('have-remote-offer',
                                             callee.signalingState);
                                callee.createAnswer(
                                  function (answer) {
                                    onAnswerCreated_(answer, caller, callee);
                                  },
                                  function(error) {
                                  }
                                );
                              },
                              failTest);
}

/**
 * @private
 */
function onAnswerCreated_(answer, caller, callee) {
  answer.sdp = transformSdp(answer.sdp);
  console.log('Answer:\n' + answer.sdp);
  callee.setLocalDescription(answer,
                             function () {
                               assertEquals('stable', callee.signalingState);
                             },
                             onLocalDescriptionError);
  receiveAnswer_(answer.sdp, caller);
}

/**
 * @private
 */
function receiveAnswer_(answerSdp, caller) {
  console.log("Receiving answer...");
  answerSdp = transformRemoteSdp(answerSdp);
  var parsedAnswer = new RTCSessionDescription({ type: 'answer',
                                                 sdp: answerSdp });
  caller.setRemoteDescription(parsedAnswer,
                              function() {
                                assertEquals('stable', caller.signalingState);
                              },
                              failTest);
}

/**
 * @private
 */
function connectOnIceCandidate_(caller, callee) {
  caller.onicecandidate = function(event) {
    onIceCandidate_(event, caller, callee);
  }
  callee.onicecandidate = function(event) {
    onIceCandidate_(event, callee, caller);
  }
}

/**
 * @private
 */
function onIceCandidate_(event, originator, target) {
  if (event.candidate) {
    var candidate = new RTCIceCandidate(event.candidate);
    target.addIceCandidate(candidate);
  } else {
    // The spec guarantees that the special "null" candidate will be fired
    // *after* changing the gathering state to "complete".
    assertEquals('complete', originator.iceGatheringState);
  }
}
