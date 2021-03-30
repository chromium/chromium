import {EyeDropperChooser, EyeDropperChooserReceiver} from '/gen/third_party/blink/public/mojom/choosers/color_chooser.mojom.m.js';

class MockEyeDropperChooser {
  constructor() {
    this.receiver_ = new EyeDropperChooserReceiver(this);
    this.interceptor_ =
        new MojoInterfaceInterceptor(EyeDropperChooser.$interfaceName);
    this.interceptor_.oninterfacerequest =
        e => this.receiver_.$.bindHandle(e.handle);
    this.interceptor_.start();

    this.receiver_.onConnectionError.addListener(() => {
      this.count_--;
    });
    this.count_ = 0;
    this.shownResolvers_ = [];
  }

  choose() {
    this.count_++;
    if (this.count_ == 1) {
      this.shownResolvers_.forEach(r => r());
    }
    return new Promise((resolve, reject) => {
      // TODO(crbug.com/992297): handle value chosen.
    });
  }

  async waitUntilShown() {
    if (this.count_ > 0) {
      return;
    }
    return new Promise(resolve => {
      this.shownResolvers_.push(resolve);
    });
  }
}

let mockEyeDropperChooser = new MockEyeDropperChooser();

export async function waitUntilEyeDropperShown() {
  return mockEyeDropperChooser.waitUntilShown();
}
