// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $Document = require('safeMethods').SafeMethods.$Document;
var $HTMLElement = require('safeMethods').SafeMethods.$HTMLElement;
var $Node = require('safeMethods').SafeMethods.$Node;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;

function AppViewImpl(appviewElement) {
  $Function.call(GuestViewContainer, this, appviewElement, 'appview');

  this.app = '';
  this.data = '';
}

AppViewImpl.prototype.__proto__ = GuestViewContainer.prototype;

AppViewImpl.prototype.getErrorNode = function() {
  if (!this.errorNode) {
    this.errorNode = $Document.createElement(document, 'div');
    $HTMLElement.innerText.set(this.errorNode, 'Unable to connect to app.');
    var style = $HTMLElement.style.get(this.errorNode);
    $Object.defineProperty(style, 'position', {value: 'absolute'});
    $Object.defineProperty(style, 'left', {value: '0px'});
    $Object.defineProperty(style, 'top', {value: '0px'});
    $Object.defineProperty(style, 'width', {value: '100%'});
    $Object.defineProperty(style, 'height', {value: '100%'});
    $Node.appendChild(this.shadowRoot, this.errorNode);
  }
  return this.errorNode;
};

AppViewImpl.prototype.buildContainerParams = function() {
  var params = $Object.create(null);
  params.appId = this.app;
  params.data = this.data || {};
  return params;
};

AppViewImpl.prototype.connect = function(app, data, callback) {
  if (!this.elementAttached) {
    if (callback) {
      callback(false);
    }
    return;
  }

  this.app = app;
  this.data = data;

  this.guest.destroy($Function.bind(this.prepareForReattach, this));
  this.guest.create(this.buildParams(), $Function.bind(function() {
    if (!this.guest.getId()) {
      var errorMsg = 'Unable to connect to app "' + app + '".';
      window.console.warn(errorMsg);
      $HTMLElement.innerText.set(this.getErrorNode(), errorMsg);
      if (callback) {
        callback(false);
      }
      return;
    }
    this.attachWindow();
    if (callback) {
      callback(true);
    }
  }, this));
};

// Exports.
exports.$set('AppViewImpl', AppViewImpl);
