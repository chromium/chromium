// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The set of scripts to be injected into the web view as early as possible.
goog.provide('__crWeb.webViewBundle');

goog.require('__crWeb.autofill');
goog.require('__crWeb.fill');
goog.require('__crWeb.form');
goog.require('__crWeb.formHandlers');
goog.require('__crWeb.passwords');
goog.require('__crWeb.suggestion');
