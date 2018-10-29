'use strict';

class MockBadgeService {
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.BadgeService);
    this.interceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.BadgeService.name);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();
  }

  init_(expectCalled) {
    this.expectCalled_ = expectCalled;
    return new Promise((resolve, reject) => {
      this.reject_ = reject;
      this.resolve_ = resolve;
    });
  }

  setBadge(contents) {
    try {
      assert_equals(this.expectCalled_, 'setBadge');
      assert_equals(contents, undefined);
      this.resolve_();
    } catch (error) {
      this.reject_(error);
    }
  }

  clearBadge() {
    try {
      assert_equals(this.expectCalled_, 'clearBadge');
      this.resolve_();
    } catch (error) {
      this.reject_(error);
    }
  }
}

let mockBadgeService = new MockBadgeService();

function callAndObserveErrors(func, expectedErrorName) {
  return new Promise((resolve, reject) => {
    try {
      func();
    } catch (error) {
      try {
        assert_equals(error.name, expectedErrorName);
        resolve();
      } catch (reason) {
        reject(reason);
      }
    }
  });
}

function badge_test(func, expectCalled, expectError) {
  promise_test(() => {
    let mockPromise = mockBadgeService.init_(expectCalled);
    return Promise.race([callAndObserveErrors(func, expectError), mockPromise]);
  });
}
