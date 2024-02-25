// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The <appview> custom element.

var registerElement = require('guestViewContainerElement').registerElement;
var forwardApiMethods = require('guestViewContainerElement').forwardApiMethods;
var GuestViewContainerElement =
    require('guestViewContainerElement').GuestViewContainerElement;
var AppViewImpl = require('appView').AppViewImpl;

class AppViewElement extends GuestViewContainerElement {
  constructor() {
    super();
    privates(this).internal = new AppViewImpl(this);
  }
}

forwardApiMethods(
    AppViewElement, AppViewImpl, null, ['connect'],
    /*promiseApiDetails=*/[]);

registerElement('AppView', AppViewElement);
