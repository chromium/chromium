'use strict';

class MockBadgeService {
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.BadgeService);
    this.interceptor_ = new MojoInterfaceInterceptor(
        blink.mojom.BadgeService.name, "context", true);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();
  }

  init_(expectedAction) {
    this.expectedAction = expectedAction;
    return new Promise((resolve, reject) => {
      this.reject_ = reject;
      this.resolve_ = resolve;
    });
  }

  setBadge(value) {
    // Accessing number when the union is a flag will throw, so read the
    // value in a try catch.
    let number;
    try {
      number = value.number;
    } catch (error) {
      number = undefined;
    }

    try {
      const action = number === undefined ? 'flag' : 'number:' + number;
      assert_equals(action, this.expectedAction);
      this.resolve_();
    } catch (error) {
      this.reject_(error);
    }
  }

  clearBadge() {
    try {
      assert_equals('clear', this.expectedAction);
      this.resolve_();
    } catch (error) {
      this.reject_(error);
    }
  }
}

let mockBadgeService = new MockBadgeService();

function badge_test(func, expectedAction, expectedError) {
  promise_test(async () => {
    let mockPromise = mockBadgeService.init_(expectedAction);

    try {
      await func();
    } catch (error) {
      return assert_equals(error.name, expectedError);
    }

    await mockPromise;
  });
}
