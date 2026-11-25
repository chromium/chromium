// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import Lottie
import UIKit

// Text provider for a LottieAnimation returning the text from a dictionary
// mapping key paths to strings.
private class TextValueProvider: AnimationKeypathTextProvider {
  private let dictionary: [String: String]
  init(_ dictionary: [String: String]) {
    self.dictionary = dictionary
  }

  func text(for keypath: AnimationKeypath, sourceText: String) -> String? {
    return dictionary[keypath.string]
  }
}

// Wrapper around LottieAnimation used to provide an interface compatible
// with the one exposed by Chrome's Lottie provider API.
@objcMembers
public class ChromiumLottieAnimation: NSObject {
  @nonobjc
  private let lottieAnimationView: LottieAnimationView

  // Designated initializer.
  public init(animationName: String, bundle: Bundle, forceUseMainThread: Bool) {
    var configuration = LottieConfiguration()
    if forceUseMainThread {
      configuration.renderingEngine = .mainThread
    }
    lottieAnimationView = LottieAnimationView(
      name: animationName, bundle: bundle, configuration: configuration)
  }

  // The animation view.
  public var animationView: UIView {
    return lottieAnimationView
  }

  // Whether the animation should stop.
  public var shouldLoop: Bool = false {
    didSet {
      lottieAnimationView.loopMode = shouldLoop ? .loop : .playOnce
    }
  }

  // Whether the animation is playing.
  public var isPlaying: Bool {
    return lottieAnimationView.isAnimationPlaying
  }

  // Sets the color value for a key path.
  public func setColorValue(_ color: UIColor, forKeypath keypath: String) {
    lottieAnimationView.setValueProvider(
      ColorValueProvider(color.lottieColorValue), keypath: AnimationKeypath(stringLiteral: keypath))
  }

  // Sets the gradient value for a key path.
  public func setGradientValue(_ colors: [UIColor], forKeypath keypath: String) {
    lottieAnimationView.setValueProvider(
      GradientValueProvider(colors.map { $0.lottieColorValue }),
      keypath: AnimationKeypath(stringLiteral: keypath))
  }

  // Sets the dictionary text provider.
  public func setDictionaryTextProvider(_ dictionary: [String: String]) {
    lottieAnimationView.textProvider = TextValueProvider(dictionary)
  }

  // Plays the animation.
  public func play() {
    lottieAnimationView.play()
  }

  // Stops the animation.
  public func stop() {
    lottieAnimationView.stop()
  }

  // Pauses the animation.
  public func pause() {
    lottieAnimationView.pause()
  }
}
