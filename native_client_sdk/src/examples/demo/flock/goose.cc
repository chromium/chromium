  // Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "goose.h"

namespace {
// The maximum speed of a goose.  Measured in meters/second.
const double kMaxSpeed = 2.0;

// The maximum force that can be applied to turn a goose when computing the
// aligment.  Measured in meters/second/second.
const double kMaxTurningForce = 0.05;

// The neighbour radius of a goose.  Only geese within this radius will affect
// the flocking computations of this goose.  Measured in pixels.
const double kNeighbourRadius = 64.0;

// The minimum distance that a goose can be from this goose.  If another goose
// comes within this distance of this goose, the flocking algorithm tries to
// move the geese apart.  Measured in pixels.
const double kPersonalSpace = 32.0;

// The distance at which attractors have effect on a goose's direction.
const double kAttractorRadius = 320.0;

// The goose will try to turn towards geese within this distance (computed
// during the cohesion phase).  Measured in pixels.
const double kMaxTurningDistance = 100.0;

// The weights used when computing the weighted sum the three flocking
// components.
const double kSeparationWeight = 2.0;
const double kAlignmentWeight = 1.0;
const double kCohesionWeight = 1.0;

}  // namespace


Goose::Goose() : location_(0, 0), velocity_(0, 0) {
}

Goose::Goose(const Vector2& location, const Vector2& velocity)
  : location_(location),
    velocity_(velocity) {
}

void Goose::SimulationTick(const std::vector<Goose>& geese,
                           const std::vector<Vector2>& attractors,
                           const pp::Rect& flock_box) {

  Vector2 acceleration = DesiredVector(geese, attractors);
  velocity_.Add(acceleration);

  // Limit the velocity to a maximum speed.
  velocity_.Clamp(kMaxSpeed);

  location_.Add(velocity_);

  // Wrap the goose location to the flock box.
  if (!flock_box.IsEmpty()) {
    while (location_.x() < flock_box.x())
      location_.set_x(location_.x() + flock_box.width());

    while (location_.x() >= flock_box.right())
      location_.set_x(location_.x() - flock_box.width());

    while (location_.y() < flock_box.y())
      location_.set_y(location_.y() + flock_box.height());

    while  (location_.y() >= flock_box.bottom())
      location_.set_y(location_.y() - flock_box.height());
  }
}

Vector2 Goose::DesiredVector(const std::vector<Goose>& geese,
                             const std::vector<Vector2>& attractors) {
  // Loop over all the neighbouring geese in the flock, accumulating
  // the separation mean, the alignment mean and the cohesion mean.
  int32_t separation_count = 0;
  Vector2 separation;
  int32_t align_count = 0;
  Vector2 alignment;
  int32_t cohesion_count = 0;
  Vector2 cohesion;

  for (std::vector<Goose>::const_iterator goose_it = geese.begin();
       goose_it < geese.end();
       ++goose_it) {
    const Goose& goose = *goose_it;

  // Compute the distance from this goose to its neighbour.
    Vector2 goose_delta = Vector2::Difference(
        location_, goose.location());
    double distance = goose_delta.Magnitude();

    separation_count = AccumulateSeparation(
        distance, goose_delta, &separation, separation_count);

    align_count = AccumulateAlignment(
        distance, goose, &alignment, align_count);
    cohesion_count = AccumulateCohesion(
        distance, goose, &cohesion, cohesion_count);
  }

  // Compute the means and create a weighted sum.  This becomes the goose's new
  // acceleration.
  if (separation_count > 0) {
    separation.Scale(1.0 / static_cast<double>(separation_count));
  }
  if (align_count > 0) {
    alignment.Scale(1.0 / static_cast<double>(align_count));
    // Limit the effect that alignment has on the final acceleration.  The
    // alignment component can overpower the others if there is a big
    // difference between this goose's velocity and its neighbours'.
    alignment.Clamp(kMaxTurningForce);
  }

  // Compute the effect of the attractors and blend this in with the flock
  // cohesion component.  An attractor has to be within kAttractorRadius to
  // effect the heading of a goose.
  for (size_t i = 0; i < attractors.size(); ++i) {
    Vector2 attractor_direction = Vector2::Difference(
        attractors[i], location_);
    double distance = attractor_direction.Magnitude();
    if (distance < kAttractorRadius) {
      attractor_direction.Scale(1000);  // Each attractor acts like 1000 geese.
      cohesion.Add(attractor_direction);
      cohesion_count++;
    }
  }

  // If there is a non-0 cohesion component, steer the goose so that it tries
  // to follow the flock.
  if (cohesion_count > 0) {
    cohesion.Scale(1.0 / static_cast<double>(cohesion_count));
    cohesion = TurnTowardsTarget(cohesion);
  }
  // Compute the weighted sum.
  separation.Scale(kSeparationWeight);
  alignment.Scale(kAlignmentWeight);
  cohesion.Scale(kCohesionWeight);
  Vector2 weighted_sum = cohesion;
  weighted_sum.Add(alignment);
  weighted_sum.Add(separation);
  return weighted_sum;
}

Vector2 Goose::TurnTowardsTarget(const Vector2& target) {
  Vector2 desired_direction = Vector2::Difference(target, location_);
  double distance = desired_direction.Magnitude();
  Vector2 new_direction;
  if (distance > 0.0) {
    desired_direction.Normalize();
    // If the target is within the turning affinity distance, then make the
    // desired direction based on distance to the target.  Otherwise, base
    // the desired direction on MAX_SPEED.
    if (distance < kMaxTurningDistance) {
      // Some pretty arbitrary dampening.
      desired_direction.Scale(kMaxSpeed * distance / 100.0);
    } else {
      desired_direction.Scale(kMaxSpeed);
    }
    new_direction = Vector2::Difference(desired_direction, velocity_);
    new_direction.Clamp(kMaxTurningForce);
  }
  return new_direction;
}

int32_t Goose::AccumulateSeparation(double distance,
                                    const Vector2& goose_delta,
                                    Vector2* separation, /* inout */
                                    int32_t separation_count) {
  if (distance > 0.0 && distance < kPersonalSpace) {
    Vector2 weighted_direction = goose_delta;
    weighted_direction.Normalize();
    weighted_direction.Scale(1.0  / distance);
    separation->Add(weighted_direction);
    separation_count++;
  }
  return separation_count;
}

int32_t Goose::AccumulateAlignment(double distance,
                                   const Goose& goose,
                                   Vector2* alignment, /* inout */
                                   int32_t align_count) {
  if (distance > 0.0 && distance < kNeighbourRadius) {
    alignment->Add(goose.velocity());
    align_count++;
  }
  return align_count;
}

int32_t Goose::AccumulateCohesion(double distance,
                                  const Goose& goose,
                                  Vector2* cohesion, /* inout */
                                  int32_t cohesion_count) {
  if (distance > 0.0 && distance < kNeighbourRadius) {
    cohesion->Add(goose.location());
    cohesion_count++;
  }
  return cohesion_count;
}
