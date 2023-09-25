// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package androidx.window.layout;

/**
 * Placeholder so that things that use it compile.
 */
public interface FoldingFeature extends DisplayFeature {
  public class OcclusionType {
      public static final OcclusionType NONE = null;
      public static final OcclusionType FULL = null;
  }
  public class Orientation {
      public static final Orientation VERTICAL = null;
      public static final Orientation HORIZONTAL = null;
  }
  public class State {
      public static final State FLAT = null;
      public static final State HALF_OPENED = null;
  }

  boolean isSeparating();
  OcclusionType getOcclusionType();
  Orientation getOrientation();
  State getState();
}
