// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assert = requireNative('assert');
var AssertTrue = assert.AssertTrue;
var AssertFalse = assert.AssertFalse;
var utils = require('utils');

function testSuperClass() {
  function SuperClassImpl() {}

  SuperClassImpl.prototype = {
    attrA: 'aSuper',
    attrB: 'bSuper',
    func: function() { return 'func'; },
    superFunc: function() { return 'superFunc'; }
  };

  function SubClassImpl() {
    SuperClassImpl.call(this);
  }

  SubClassImpl.prototype = {
    __proto__: SuperClassImpl.prototype,
    attrA: 'aSub',
    attrC: 'cSub',
    func: function() { return 'overridden'; },
    subFunc: function() { return 'subFunc'; }
  };

  function SuperClass() {
    privates(SuperClass).constructPrivate(this, arguments);
  }
  utils.expose(SuperClass, SuperClassImpl, {
    functions: ['func', 'superFunc'],
    properties: ['attrA', 'attrB'],
  });

  function SubClass() {
    privates(SubClass).constructPrivate(this, arguments);
  }
  utils.expose(SubClass, SubClassImpl, {
    superclass: SuperClass,
    functions: ['subFunc'],
    properties: ['attrC'],
  });

  var supe = new SuperClass();
  AssertTrue(supe.attrA == 'aSuper');
  AssertTrue(supe.attrB == 'bSuper');
  AssertFalse('attrC' in supe);
  AssertTrue(supe.func() == 'func');
  AssertTrue('superFunc' in supe);
  AssertTrue(supe.superFunc() == 'superFunc');
  AssertFalse('subFunc' in supe);
  AssertTrue(supe instanceof SuperClass);

  var sub = new SubClass();
  AssertTrue(sub.attrA == 'aSub');
  AssertTrue(sub.attrB == 'bSuper');
  AssertTrue(sub.attrC == 'cSub');
  AssertTrue(sub.func() == 'overridden');
  AssertTrue(sub.superFunc() == 'superFunc');
  AssertTrue('subFunc' in sub);
  AssertTrue(sub.subFunc() == 'subFunc');
  AssertTrue(sub instanceof SuperClass);
  AssertTrue(sub instanceof SubClass);

  function SubSubClassImpl() {}
  SubSubClassImpl.prototype = Object.create(SubClassImpl.prototype);
  SubSubClassImpl.prototype.subSubFunc = function() { return 'subsub'; };

  function SubSubClass() {
    privates(SubSubClass).constructPrivate(this, arguments);
  }
  utils.expose(SubSubClass, SubSubClassImpl, {
    superclass: SubClass,
    functions: ['subSubFunc'],
  });

  var subsub = new SubSubClass();
  AssertTrue(subsub.attrA == 'aSub');
  AssertTrue(subsub.attrB == 'bSuper');
  AssertTrue(subsub.attrC == 'cSub');
  AssertTrue(subsub.func() == 'overridden');
  AssertTrue(subsub.superFunc() == 'superFunc');
  AssertTrue(subsub.subFunc() == 'subFunc');
  AssertTrue(subsub.subSubFunc() == 'subsub');
  AssertTrue(subsub instanceof SuperClass);
  AssertTrue(subsub instanceof SubClass);
  AssertTrue(subsub instanceof SubSubClass);
}

exports.testSuperClass = testSuperClass;
