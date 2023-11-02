// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_DEMO_FLOCK_GOOSE_H_
#define EXAMPLES_DEMO_FLOCK_GOOSE_H_

#include <vector>
#include "ppapi/cpp/rect.h"

#include "vector2.h"

// A Goose.  Each goose has a location and a velocity.  Implements the
// flocking algortihm described here:
// http://processingjs.org/learning/topic/flocking with references to
// http://harry.me/2011/02/17/neat-algorithms---flocking.
class Goose {
 public:
  // Initialize a Goose at location (0, 0) no velocity.
  Goose();

  // Initialize a Goose at the given location with the specified velocity.
  Goose(const Vector2& location, const Vector2& velocity);

  // Run one tick of the simulation.  Compute a new acceleration based on the
  // flocking algorithm (see Goose.flock()) and update the goose's location
  // by integrating acceleration and velocity.
  // @param geese The list of all the geese in the flock.
  // @param  attractors The list of attractors.  Geese have affinity for these
  //     points.
  // @param flockBox The geese will stay inside of this box.  If the flock_box
  //     is empty, the geese don't have boundaries.
  void SimulationTick(const std::vector<Goose>& geese,
                      const std::vector<Vector2>& attractors,
                      const pp::Rect& flock_box);

  // Implement the flocking algorithm in five steps:
  //  1. Compute the separation component,
  //  2. Compute the alignment component,
  //  3. Compute the cohesion component.
  //  4. Compute the effect of the attractors and blend this in with the
  //     cohesion component.
  //  5. Create a weighted sum of the three components and use this as the
  //     new acceleration for the goose.
  // This is an O(n^2) version of the algorithm.  There are ways to speed this
  // up using spatial coherence techniques, but this version is much simpler.
  // @param geese The list of all the neighbouring geese (in this
  //     implementation, this is all the geese in the flock).
  // @param  attractors The list of attractors.  Geese have affinity for these
  //     points.
  // @return The acceleration vector for this goose based on the flocking
  //     algorithm.
  Vector2 DesiredVector(const std::vector<Goose>& geese,
                        const std::vector<Vector2>& attractors);

  // Turn the goose towards a target.  The amount of turning force is clamped
  // to |kMaxTurningForce|.
  // @param target Turn the goose towards this target.
  // @return A vector representing the new direction of the goose.
  Vector2 TurnTowardsTarget(const Vector2& target);

  // Accessors for location and velocoity.
  Vector2 location() const {
    return location_;
  }
  Vector2 velocity() const {
    return velocity_;
  }

 private:
  // Add a neighbouring goose's contribution to the separation mean.  Only
  // consider geese that have moved inside of this goose's personal space.
  // Modifies the separation accumulator |separation| in-place.
  // @param distance The distance from this goose to the neighbouring goose.
  // @param gooseDirection The direction vector from this goose to the
  //     neighbour.
  // @param separation The accumulated separation from all the neighbouring
  //     geese.
  // @param separationCount The current number of geese that have contributed to
  //     the separation component so far.
  // @return The new count of geese that contribute to the separation component.
  //     If the goose under consideration does not contribute, this value is the
  //     same as |separationCount|.
  int32_t AccumulateSeparation(double distance,
                               const Vector2& goose_direction,
                               Vector2* separation, /* inout */
                               int32_t separation_count);

  // Add a neighbouring goose's contribution to the alignment mean.  Alignment
  // is the average velocity of the neighbours. Only consider geese that are
  // within |kNeighbourRadius|.  Modifies the alignment accumulator |alignment|
  // in-place.
  // @param distance The distance from this goose to the neighbouring goose.
  // @param goose The neighbouring goose under consideration.
  // @param alignment The accumulated alignment from all the neighbouring geese.
  // @param alignCount The current number of geese that have contributed to the
  //     alignment component so far.
  // @return The new count of geese that contribute to the alignment component.
  // If the goose under consideration does not contribute, this value is the
  //     same as |alignCount|.
  int32_t AccumulateAlignment(double distance,
                              const Goose& goose,
                              Vector2* alignment, /* inout */
                              int32_t align_count);

  // Add a neighbouring goose's contribution to the cohesion mean.  Cohesion is
  // based on the average location of the neighbours.  The goose attempts to
  // point to this average location.  Only consider geese that are within
  // |kNeighbourRadius|.  Modifies the cohesion accumulator |cohesion| in-place.
  // @param {!number} distance The distance from this goose to the neighbouring
  //     goose.
  // @param {!Goose} goose The neighbouring goose under consideration.
  // @param {!goog.math.Vec2} cohesion The accumulated cohesion from all the
  //     neighbouring geese.
  // @param {!number} cohesionCount The current number of geese that have
  //     contributed to the cohesion component so far.
  // @return {!number} The new count of geese that contribute to the cohesion
  //     component.  If the goose under consideration does not contribute, this
  //     value is the same as |cohesionCount|.
  int32_t AccumulateCohesion(double distance,
                             const Goose& goose,
                             Vector2* cohesion, /* inout */
                             int32_t cohesion_count);

  Vector2 location_;
  Vector2 velocity_;
};

#endif  // EXAMPLES_DEMO_FLOCK_GOOSE_H_
