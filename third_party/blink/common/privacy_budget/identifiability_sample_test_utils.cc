#include "third_party/blink/public/common/privacy_budget/identifiability_sample_test_utils.h"

namespace blink {

bool CountingSettingsProvider::IsMetaExperimentActive() const {
  ++state_->count_of_is_meta_experiment_active;
  return state_->response_for_is_meta_experiment_active;
}

bool CountingSettingsProvider::IsActive() const {
  ++state_->count_of_is_active;
  return state_->response_for_is_active;
}

bool CountingSettingsProvider::IsAnyTypeOrSurfaceBlocked() const {
  ++state_->count_of_is_any_type_or_surface_blocked;
  return state_->response_for_is_anything_blocked;
}

bool CountingSettingsProvider::IsSurfaceAllowed(
    IdentifiableSurface surface) const {
  ++state_->count_of_is_surface_allowed;
  return state_->response_for_is_allowed;
}

bool CountingSettingsProvider::IsTypeAllowed(
    IdentifiableSurface::Type type) const {
  ++state_->count_of_is_type_allowed;
  return state_->response_for_is_allowed;
}

}  // namespace blink
