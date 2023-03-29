// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Default transform functions, overridden by some test cases.
var transformSdp = function(sdp) { return sdp; };
var transformRemoteSdp = function(sdp) { return sdp; };

// Provide functions to set the transform functions.
function setOfferSdpTransform(newTransform) {
  transformSdp = newTransform;
}
function setRemoteSdpTransform(newTransform) {
  transformRemoteSdp = newTransform;
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
async function negotiateBetween(caller, callee) {
  console.log("Negotiating call...");
  // Not stable = negotiation is ongoing. The behavior of re-negotiating while
  // a negotiation is ongoing is more or less undefined, so avoid this.
  if (caller.signalingState != 'stable' || callee.signalingState != 'stable')
    throw 'You can only negotiate when the connection is stable!';

  connectOnIceCandidate_(caller, callee);

  const offer = await new Promise((resolve, reject) => {
    caller.createOffer(resolve, reject);
  })
  return onOfferCreated_(offer, caller, callee);
}

/**
 * @private
 */
async function onOfferCreated_(offer, caller, callee) {
  offer.sdp = transformSdp(offer.sdp);
  console.log('Offer:\n' + offer.sdp);
  await new Promise((resolve, reject) => {
    caller.setLocalDescription(offer, resolve, reject);
  });
  assertEquals('have-local-offer', caller.signalingState);
  return receiveOffer_(offer.sdp, caller, callee);
}

/**
 * @private
 */
async function receiveOffer_(offerSdp, caller, callee) {
  console.log("Receiving offer...");
  offerSdp = transformRemoteSdp(offerSdp);

  var parsedOffer = new RTCSessionDescription({ type: 'offer',
                                                sdp: offerSdp });
  await new Promise((resolve, reject) => {
    callee.setRemoteDescription(parsedOffer, resolve, reject);
  });
  assertEquals('have-remote-offer', callee.signalingState);
  const answer = await new Promise((resolve, reject) => {
    callee.createAnswer(resolve, reject);
  });
  return onAnswerCreated_(answer, caller, callee);
}

/**
 * @private
 */
async function onAnswerCreated_(answer, caller, callee) {
  answer.sdp = transformSdp(answer.sdp);
  console.log('Answer:\n' + answer.sdp);
  await new Promise((resolve, reject) => {
    callee.setLocalDescription(answer, resolve, reject);
  });
  assertEquals('stable', callee.signalingState);
  return receiveAnswer_(answer.sdp, caller);
}

/**
 * @private
 */
async function receiveAnswer_(answerSdp, caller) {
  console.log("Receiving answer...");
  answerSdp = transformRemoteSdp(answerSdp);
  var parsedAnswer = new RTCSessionDescription({ type: 'answer',
                                                 sdp: answerSdp });
  await new Promise((resolve, reject) => {
    caller.setRemoteDescription(parsedAnswer, resolve, reject);
  });
  assertEquals('stable', caller.signalingState);
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
async function onIceCandidate_(event, originator, target) {
  if (event.candidate) {
    var candidate = new RTCIceCandidate(event.candidate);
    target.addIceCandidate(candidate);
  } else {
    // The spec guarantees that the special "null" candidate will be fired
    // *after* changing the gathering state to "complete".
    assertEquals('complete', originator.iceGatheringState);
  }
}
