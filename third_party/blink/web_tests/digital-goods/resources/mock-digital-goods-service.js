'use strict';

class MockDigitalGoods {
  constructor() {
    this.resetRecordedAction_();
  }

  bind(request) {
    this.binding = new mojo.Binding(payments.mojom.DigitalGoods, this, request);
  }

  getRecordedAction_() {
    return this.action;
  }

  resetRecordedAction_() {
    this.action = new Promise((resolve, reject) => {
      this.actionResolve_ = resolve;
    });
  }

  makeItemDetails_(id) {
    // itemDetails is a payments.mojom.ItemDetails.
    let itemDetails = {};
    itemDetails.itemId = id;
    itemDetails.title = 'title:' + id;
    itemDetails.description = 'description:' + id;
    // price is a payments.mojom.PaymentCurrencyAmount.
    itemDetails.price = {};
    itemDetails.price.currency = 'AUD';
    // Set price.value as on number in |id| dollars.
    const matchNum = id.match(/\d+/);
    const num = matchNum ? matchNum[0] : 0;
    itemDetails.price.value = num + '.00';
    if (num % 2) {
      // Add optional fields.
      itemDetails.subscriptionPeriod = 'P' + num + 'Y';
      itemDetails.freeTrialPeriod = 'P' + num + 'M';
      itemDetails.introductoryPrice = {};
      itemDetails.introductoryPrice.currency = 'JPY';
      itemDetails.introductoryPrice.value = 2*num + '';
      itemDetails.introductoryPricePeriod = 'P' + num + 'D';
    }
    return itemDetails;
  }

  async getDetails(itemIds) {
    this.actionResolve_('getDetails:' + itemIds);

    // Simulate some backend failure response.
    if (itemIds.includes('fail')) {
      return {code: /*BillingResponseCode.kError=*/1, itemDetailsList: []};
    }

    let itemDetailsList = [];
    // Simulate some specified IDs are not found.
    const found = itemIds.filter(id => !id.includes('gone'));
    for (const id of found) {
      itemDetailsList.push(this.makeItemDetails_(id));
    }

    return {
      code: /*BillingResponseCode.kOk=*/0,
      itemDetailsList: itemDetailsList
    };
  }

  async acknowledge(purchaseToken, makeAvailableAgain) {
    this.actionResolve_(
        'acknowledge:' + purchaseToken + ' ' + makeAvailableAgain);

    if (purchaseToken === 'fail') {
      return {code: /*BillingResponseCode.kError=*/1};
    }
    return {code: /*BillingResponseCode.kOk=*/0};
  }
}

let mockDigitalGoods = new MockDigitalGoods();


class MockDigitalGoodsFactory {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(
            payments.mojom.DigitalGoodsFactory.name);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.bindingSet_ = new mojo.BindingSet(payments.mojom.DigitalGoodsFactory);

    this.interceptor_.start();
  }

  bind(handle) {
    this.bindingSet_.addBinding(this, handle);
  }

  async createDigitalGoods(paymentMethod) {
    if (paymentMethod !== 'https://play.google.com/billing') {
      return {
        code: /*CreateDigitalGoodsResponseCode.kUnsupportedPaymentMethod=*/2,
        digitalGoods: null
      };
    }

    let digitalGoodsPtr = new payments.mojom.DigitalGoodsPtr();
    mockDigitalGoods.bind(mojo.makeRequest(digitalGoodsPtr));

    return {
      code: /*CreateDigitalGoodsResponseCode.kOk=*/0,
      digitalGoods: digitalGoodsPtr
    };
  }
}

let mockDigitalGoodsFactory = new MockDigitalGoodsFactory();

function digital_goods_test(func, {
  title,
  expectedAction,
  paymentMethod = 'https://play.google.com/billing',
} = {}) {
  promise_test(async () => {
    mockDigitalGoods.resetRecordedAction_();
    const service = await window.getDigitalGoodsService(paymentMethod);

    await func(service);

    if (expectedAction) {
      const action = await mockDigitalGoods.getRecordedAction_();
      assert_equals(action, expectedAction);
    }
  }, title);
}
