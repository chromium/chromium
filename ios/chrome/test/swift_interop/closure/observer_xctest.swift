// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Combine
import CxxImports
import XCTest

// A subject whose observer is a closure
class PlainValueSubject {
  var valueDidChangeCallback: ((Int32) -> Void)?
  var value: Int32 = 0 {
    didSet {
      valueDidChangeCallback?(value)
    }
  }
}

// A subject that directly invokes an observer that is a
// base::RepeatingCallback<void(int)>
class CxxCallbackValueSubject {
  var valueDidChangeCallback: ValueObserver.ValueDidChangeCallback?
  var value: Int32 = 0 {
    didSet {
      valueDidChangeCallback?(value)
    }
  }
}

// A subject that uses a Combine Publisher to invoke an observer
// that is a base::RepeatingCallback<void(int)>
class PublishedValueSubject {
  @Published
  var value: Int32 = 0

  var subscriptions: [AnyCancellable] = []

  func registerObserver(observer: ValueObserver.ValueDidChangeCallback) {
    self.$value
      .sink { [observer] newValue in
        observer(newValue)
      }
      .store(in: &subscriptions)
  }
}

class ObserverTest: XCTestCase {

  // This is the simplest way to setup C++ code that observes swift. It has
  // the disadvantage that the subject retains a strong ref on the observer,
  // which can sometimes make object lifetimes hard to manage.
  func testPlainValueObserver() {
    let valueSubject = PlainValueSubject()
    valueSubject.value = 1
    guard let valueObserver = ValueObserver.MakeForSwift() else {
      XCTFail()
      return
    }
    XCTAssertEqual(valueObserver.value(), 0)
    valueSubject.valueDidChangeCallback = { [valueObserver] value in
      valueObserver.ValueDidChange(value)
    }
    // Observer *NOT* notified upon registration
    XCTAssertEqual(valueObserver.value(), 0)
    valueSubject.value = 2
    XCTAssertEqual(valueObserver.value(), 2)
  }

  // This approach uses a base::RepeatingCallback, which allows the observer
  // to control the retention policy by chosing how the observer instance is
  // bound to the callback. In this particular case, it uses a WeakPtr.
  func testCxxCallbackValueObserver() {
    let valueSubject = CxxCallbackValueSubject()
    valueSubject.value = 1
    guard let valueObserver = ValueObserver.MakeForSwift() else {
      XCTFail()
      return
    }
    XCTAssertEqual(valueObserver.value(), 0)
    valueSubject.valueDidChangeCallback =
      valueObserver.GetValueDidChangeCallback()
    // Observer *NOT* notified upon registration
    XCTAssertEqual(valueObserver.value(), 0)
    valueSubject.value = 2
    XCTAssertEqual(valueObserver.value(), 2)
  }

  // This way of registering an observer offers maximum flexibility and
  // customizability by leveraging the Combine framework in conjuction with
  // base::RepeatingCallback.
  func testCombinePublisherValueObserver() {
    let valueSubject = PublishedValueSubject()
    valueSubject.value = 1
    guard let valueObserver = ValueObserver.MakeForSwift() else {
      XCTFail()
      return
    }
    XCTAssertEqual(valueObserver.value(), 0)
    valueSubject.registerObserver(
      observer: valueObserver.GetValueDidChangeCallback())
    // Observer notified upon registration.
    XCTAssertEqual(valueObserver.value(), 1)
    valueSubject.value = 2
    XCTAssertEqual(valueObserver.value(), 2)
  }

  func testCombinePublisherWithMultipleObservers() {
    let valueSubject = PublishedValueSubject()
    guard let valueObserver1 = ValueObserver.MakeForSwift() else {
      XCTFail()
      return
    }
    guard let valueObserver2 = ValueObserver.MakeForSwift() else {
      XCTFail()
      return
    }
    valueSubject.registerObserver(
      observer: valueObserver1.GetValueDidChangeCallback())
    valueSubject.registerObserver(
      observer: valueObserver2.GetValueDidChangeCallback())
    valueSubject.value = 2
    XCTAssertEqual(valueObserver1.value(), 2)
    XCTAssertEqual(valueObserver2.value(), 2)
  }
}
