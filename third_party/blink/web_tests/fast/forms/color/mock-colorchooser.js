'use strict';

class MockColorChooser {
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.ColorChooserFactory);
    this.interceptor_ =
        new MojoInterfaceInterceptor(blink.mojom.ColorChooserFactory.name);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();

    this.bindingSet_.setConnectionErrorHandler(() => {
      this.count_--;
    });
    this.count_ = 0;
  }

  openColorChooser(chooser, client, color, suggestions) {
    this.count_++;
  }

  isChooserShown() {
    return this.count_ > 0;
  }
}

let mockColorChooser = new MockColorChooser();

function waitUntilChooserShown(then) {
  if (!mockColorChooser.isChooserShown())
    return setTimeout(() => { waitUntilChooserShown(then); }, 0);
  if (then)
    then();
}
function waitUntilChooserClosed(then) {
  if (mockColorChooser.isChooserShown())
    return setTimeout(() => { waitUntilChooserClosed(then); }, 0);
  if (then)
    then();
}
