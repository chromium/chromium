// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the declarativeWebRequest API.

apiBridge.registerCustomHook(function(api) {
  var declarativeWebRequest = api.compiledApi;

  // Helper function for the constructor of concrete datatypes of the
  // declarative webRequest API.
  // Makes sure that |this| contains the union of parameters and
  // {'instanceType': 'declarativeWebRequest.' + typeId} and validates the
  // generated union dictionary against the schema for |typeId|.
  function setupInstance(instance, parameters, typeId) {
    for (var key in parameters) {
      if ($Object.hasOwnProperty(parameters, key)) {
        instance[key] = parameters[key];
      }
    }

    var qualifiedType = 'declarativeWebRequest.' + typeId;
    instance.instanceType = qualifiedType;
    bindingUtil.validateType(qualifiedType, instance);
  }

  // Setup all data types for the declarative webRequest API.
  declarativeWebRequest.RequestMatcher = function(parameters) {
    setupInstance(this, parameters, 'RequestMatcher');
  };
  declarativeWebRequest.CancelRequest = function(parameters) {
    setupInstance(this, parameters, 'CancelRequest');
  };
  declarativeWebRequest.RedirectRequest = function(parameters) {
    setupInstance(this, parameters, 'RedirectRequest');
  };
  declarativeWebRequest.SetRequestHeader = function(parameters) {
    setupInstance(this, parameters, 'SetRequestHeader');
  };
  declarativeWebRequest.RemoveRequestHeader = function(parameters) {
    setupInstance(this, parameters, 'RemoveRequestHeader');
  };
  declarativeWebRequest.AddResponseHeader = function(parameters) {
    setupInstance(this, parameters, 'AddResponseHeader');
  };
  declarativeWebRequest.RemoveResponseHeader = function(parameters) {
    setupInstance(this, parameters, 'RemoveResponseHeader');
  };
  declarativeWebRequest.RedirectToTransparentImage =
      function(parameters) {
    setupInstance(this, parameters, 'RedirectToTransparentImage');
  };
  declarativeWebRequest.RedirectToEmptyDocument = function(parameters) {
    setupInstance(this, parameters, 'RedirectToEmptyDocument');
  };
  declarativeWebRequest.RedirectByRegEx = function(parameters) {
    setupInstance(this, parameters, 'RedirectByRegEx');
  };
  declarativeWebRequest.IgnoreRules = function(parameters) {
    setupInstance(this, parameters, 'IgnoreRules');
  };
  declarativeWebRequest.AddRequestCookie = function(parameters) {
    setupInstance(this, parameters, 'AddRequestCookie');
  };
  declarativeWebRequest.AddResponseCookie = function(parameters) {
    setupInstance(this, parameters, 'AddResponseCookie');
  };
  declarativeWebRequest.EditRequestCookie = function(parameters) {
    setupInstance(this, parameters, 'EditRequestCookie');
  };
  declarativeWebRequest.EditResponseCookie = function(parameters) {
    setupInstance(this, parameters, 'EditResponseCookie');
  };
  declarativeWebRequest.RemoveRequestCookie = function(parameters) {
    setupInstance(this, parameters, 'RemoveRequestCookie');
  };
  declarativeWebRequest.RemoveResponseCookie = function(parameters) {
    setupInstance(this, parameters, 'RemoveResponseCookie');
  };
  declarativeWebRequest.SendMessageToExtension = function(parameters) {
    setupInstance(this, parameters, 'SendMessageToExtension');
  };
});
