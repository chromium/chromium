// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/conversion_utils.h"
#include "base/notreached.h"

namespace extensions {
namespace api {
namespace media_perception_private {

namespace {

Metadata MetadataProtoToIdl(const mri::Metadata& metadata) {
  Metadata metadata_result;
  if (metadata.has_visual_experience_controller_version()) {
    metadata_result.visual_experience_controller_version =
        metadata.visual_experience_controller_version();
  }

  return metadata_result;
}

HotwordType HotwordTypeProtoToIdl(const mri::HotwordDetection::Type& type) {
  switch (type) {
    case mri::HotwordDetection::UNKNOWN_TYPE:
      return HotwordType::kUnknownType;
    case mri::HotwordDetection::OK_GOOGLE:
      return HotwordType::kOkGoogle;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown hotword type: " << type;
  return HotwordType::kUnknownType;
}

Hotword HotwordProtoToIdl(const mri::HotwordDetection::Hotword& hotword) {
  Hotword hotword_result;
  if (hotword.has_id())
    hotword_result.id = hotword.id();

  if (hotword.has_type())
    hotword_result.type = HotwordTypeProtoToIdl(hotword.type());

  if (hotword.has_frame_id())
    hotword_result.frame_id = hotword.frame_id();

  if (hotword.has_start_timestamp_ms()) {
    hotword_result.start_timestamp_ms = hotword.start_timestamp_ms();
  }

  if (hotword.has_end_timestamp_ms()) {
    hotword_result.end_timestamp_ms = hotword.end_timestamp_ms();
  }

  if (hotword.has_confidence())
    hotword_result.confidence = hotword.confidence();

  if (hotword.has_id())
    hotword_result.id = hotword.id();

  return hotword_result;
}

HotwordDetection HotwordDetectionProtoToIdl(
    const mri::HotwordDetection& detection) {
  HotwordDetection detection_result;

  if (detection.hotwords_size() > 0) {
    detection_result.hotwords.emplace();
    for (const auto& hotword : detection.hotwords()) {
      detection_result.hotwords->emplace_back(HotwordProtoToIdl(hotword));
    }
  }

  return detection_result;
}

AudioSpectrogram AudioSpectrogramProtoToIdl(
    const mri::AudioSpectrogram& spectrogram) {
  AudioSpectrogram spectrogram_result;
  if (spectrogram.values_size() > 0) {
    spectrogram_result.values.emplace();
    for (const auto& value : spectrogram.values()) {
      spectrogram_result.values->emplace_back(value);
    }
  }
  return spectrogram_result;
}

AudioHumanPresenceDetection AudioHumanPresenceDetectionProtoToIdl(
    const mri::AudioHumanPresenceDetection& detection) {
  AudioHumanPresenceDetection detection_result;
  if (detection.has_human_presence_likelihood()) {
    detection_result.human_presence_likelihood =
        detection.human_presence_likelihood();
  }
  if (detection.has_noise_spectrogram()) {
    detection_result.noise_spectrogram =
        AudioSpectrogramProtoToIdl(detection.noise_spectrogram());
  }
  if (detection.has_frame_spectrogram()) {
    detection_result.frame_spectrogram =
        AudioSpectrogramProtoToIdl(detection.frame_spectrogram());
  }
  return detection_result;
}

AudioLocalization AudioLocalizationProtoToIdl(
    const mri::AudioLocalization& localization) {
  AudioLocalization localization_result;
  if (localization.has_azimuth_radians()) {
    localization_result.azimuth_radians = localization.azimuth_radians();
  }
  if (localization.azimuth_scores_size() > 0) {
    localization_result.azimuth_scores.emplace();
    for (const auto& score : localization.azimuth_scores()) {
      localization_result.azimuth_scores->emplace_back(score);
    }
  }
  return localization_result;
}

AudioPerception AudioPerceptionProtoToIdl(
    const mri::AudioPerception& perception) {
  AudioPerception perception_result;
  if (perception.has_timestamp_us()) {
    perception_result.timestamp_us = perception.timestamp_us();
  }
  if (perception.has_audio_localization()) {
    perception_result.audio_localization =
        AudioLocalizationProtoToIdl(perception.audio_localization());
  }
  if (perception.has_audio_human_presence_detection()) {
    perception_result.audio_human_presence_detection =
        AudioHumanPresenceDetectionProtoToIdl(
            perception.audio_human_presence_detection());
  }
  if (perception.has_hotword_detection()) {
    perception_result.hotword_detection =
        HotwordDetectionProtoToIdl(perception.hotword_detection());
  }
  return perception_result;
}

LightCondition LightConditionProtoToIdl(
    const mri::VideoHumanPresenceDetection::LightCondition& condition) {
  switch (condition) {
    case mri::VideoHumanPresenceDetection::UNSPECIFIED:
      return LightCondition::kUnspecified;
    case mri::VideoHumanPresenceDetection::NO_CHANGE:
      return LightCondition::kNoChange;
    case mri::VideoHumanPresenceDetection::TURNED_ON:
      return LightCondition::kTurnedOn;
    case mri::VideoHumanPresenceDetection::TURNED_OFF:
      return LightCondition::kTurnedOff;
    case mri::VideoHumanPresenceDetection::DIMMER:
      return LightCondition::kDimmer;
    case mri::VideoHumanPresenceDetection::BRIGHTER:
      return LightCondition::kBrighter;
    case mri::VideoHumanPresenceDetection::BLACK_FRAME:
      return LightCondition::kBlackFrame;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown light condition: " << condition;
      return LightCondition::kUnspecified;
  }
}

VideoHumanPresenceDetection VideoHumanPresenceDetectionProtoToIdl(
    const mri::VideoHumanPresenceDetection& detection) {
  VideoHumanPresenceDetection detection_result;
  if (detection.has_human_presence_likelihood()) {
    detection_result.human_presence_likelihood =
        detection.human_presence_likelihood();
  }

  if (detection.has_motion_detected_likelihood()) {
    detection_result.motion_detected_likelihood =
        detection.motion_detected_likelihood();
  }

  if (detection.has_light_condition()) {
    detection_result.light_condition =
        LightConditionProtoToIdl(detection.light_condition());
  }

  if (detection.has_light_condition_likelihood()) {
    detection_result.light_condition_likelihood =
        detection.light_condition_likelihood();
  }

  return detection_result;
}

AudioVisualHumanPresenceDetection AudioVisualHumanPresenceDetectionProtoToIdl(
    const mri::AudioVisualHumanPresenceDetection& detection) {
  AudioVisualHumanPresenceDetection detection_result;

  if (detection.has_human_presence_likelihood()) {
    detection_result.human_presence_likelihood =
        detection.human_presence_likelihood();
  }

  return detection_result;
}

AudioVisualPerception AudioVisualPerceptionProtoToIdl(
    const mri::AudioVisualPerception& perception) {
  AudioVisualPerception perception_result;
  if (perception.has_timestamp_us()) {
    perception_result.timestamp_us = perception.timestamp_us();
  }
  if (perception.has_audio_visual_human_presence_detection()) {
    perception_result.audio_visual_human_presence_detection =
        AudioVisualHumanPresenceDetectionProtoToIdl(
            perception.audio_visual_human_presence_detection());
  }
  return perception_result;
}

Point PointProtoToIdl(const mri::Point& point) {
  Point point_result;
  if (point.has_x())
    point_result.x = point.x();

  if (point.has_y())
    point_result.y = point.y();

  return point_result;
}

void PointIdlToProto(const Point& point, mri::Point* point_result) {
  if (point.x)
    point_result->set_x(*point.x);

  if (point.y)
    point_result->set_y(*point.y);
}

BoundingBox BoundingBoxProtoToIdl(const mri::BoundingBox& bounding_box) {
  BoundingBox bounding_box_result;
  if (bounding_box.has_normalized()) {
    bounding_box_result.normalized = bounding_box.normalized();
  }

  if (bounding_box.has_top_left())
    bounding_box_result.top_left = PointProtoToIdl(bounding_box.top_left());

  if (bounding_box.has_bottom_right()) {
    bounding_box_result.bottom_right =
        PointProtoToIdl(bounding_box.bottom_right());
  }

  return bounding_box_result;
}

DistanceUnits DistanceUnitsProtoToIdl(const mri::Distance& distance) {
  if (distance.has_units()) {
    switch (distance.units()) {
      case mri::Distance::METERS:
        return DistanceUnits::kMeters;
      case mri::Distance::PIXELS:
        return DistanceUnits::kPixels;
      case mri::Distance::UNITS_UNSPECIFIED:
        return DistanceUnits::kUnspecified;
    }
    NOTREACHED_IN_MIGRATION() << "Unknown distance units: " << distance.units();
  }
  return DistanceUnits::kUnspecified;
}

Distance DistanceProtoToIdl(const mri::Distance& distance) {
  Distance distance_result;
  distance_result.units = DistanceUnitsProtoToIdl(distance);

  if (distance.has_magnitude())
    distance_result.magnitude = distance.magnitude();

  return distance_result;
}

FramePerceptionType FramePerceptionTypeProtoToIdl(int type) {
  switch (type) {
    case mri::FramePerception::UNKNOWN_TYPE:
      return FramePerceptionType::kUnknownType;
    case mri::FramePerception::FACE_DETECTION:
      return FramePerceptionType::kFaceDetection;
    case mri::FramePerception::PERSON_DETECTION:
      return FramePerceptionType::kPersonDetection;
    case mri::FramePerception::MOTION_DETECTION:
      return FramePerceptionType::kMotionDetection;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown frame perception type: " << type;
  return FramePerceptionType::kUnknownType;
}

EntityType EntityTypeProtoToIdl(const mri::Entity& entity) {
  if (entity.has_type()) {
    switch (entity.type()) {
      case mri::Entity::FACE:
        return EntityType::kFace;
      case mri::Entity::PERSON:
        return EntityType::kPerson;
      case mri::Entity::MOTION_REGION:
        return EntityType::kMotionRegion;
      case mri::Entity::LABELED_REGION:
        return EntityType::kLabeledRegion;
      case mri::Entity::UNSPECIFIED:
        return EntityType::kUnspecified;
    }
    NOTREACHED_IN_MIGRATION() << "Unknown entity type: " << entity.type();
  }
  return EntityType::kUnspecified;
}

Entity EntityProtoToIdl(const mri::Entity& entity) {
  Entity entity_result;
  if (entity.has_id())
    entity_result.id = entity.id();

  entity_result.type = EntityTypeProtoToIdl(entity);
  if (entity.has_confidence())
    entity_result.confidence = entity.confidence();

  if (entity.has_bounding_box())
    entity_result.bounding_box = BoundingBoxProtoToIdl(entity.bounding_box());

  if (entity.has_depth())
    entity_result.depth = DistanceProtoToIdl(entity.depth());

  if (entity.has_label())
    entity_result.entity_label = entity.label();

  return entity_result;
}

PacketLatency PacketLatencyProtoToIdl(
    const mri::PacketLatency& packet_latency) {
  PacketLatency packet_latency_result;

  if (packet_latency.has_label()) {
    packet_latency_result.packet_label = packet_latency.label();
  }

  if (packet_latency.has_latency_usec()) {
    packet_latency_result.latency_usec = packet_latency.latency_usec();
  }

  return packet_latency_result;
}

FramePerception FramePerceptionProtoToIdl(
    const mri::FramePerception& frame_perception) {
  FramePerception frame_perception_result;
  if (frame_perception.has_frame_id()) {
    frame_perception_result.frame_id = frame_perception.frame_id();
  }
  if (frame_perception.has_frame_width_in_px()) {
    frame_perception_result.frame_width_in_px =
        frame_perception.frame_width_in_px();
  }
  if (frame_perception.has_frame_height_in_px()) {
    frame_perception_result.frame_height_in_px =
        frame_perception.frame_height_in_px();
  }
  if (frame_perception.has_timestamp()) {
    frame_perception_result.timestamp = frame_perception.timestamp();
  }
  if (frame_perception.entity_size() > 0) {
    frame_perception_result.entities.emplace();
    for (const auto& entity : frame_perception.entity())
      frame_perception_result.entities->emplace_back(EntityProtoToIdl(entity));
  }
  if (frame_perception.packet_latency_size() > 0) {
    frame_perception_result.packet_latency.emplace();
    for (const auto& packet_latency : frame_perception.packet_latency()) {
      frame_perception_result.packet_latency->emplace_back(
          PacketLatencyProtoToIdl(packet_latency));
    }
  }
  if (frame_perception.has_video_human_presence_detection()) {
    frame_perception_result.video_human_presence_detection =
        VideoHumanPresenceDetectionProtoToIdl(
            frame_perception.video_human_presence_detection());
  }
  if (frame_perception.perception_types_size() > 0) {
    frame_perception_result.frame_perception_types.emplace();
    for (const auto& type : frame_perception.perception_types()) {
      frame_perception_result.frame_perception_types->emplace_back(
          FramePerceptionTypeProtoToIdl(type));
    }
  }
  return frame_perception_result;
}

ImageFormat ImageFormatProtoToIdl(const mri::ImageFrame& image_frame) {
  if (image_frame.has_format()) {
    switch (image_frame.format()) {
      case mri::ImageFrame::RGB:
        return ImageFormat::kRaw;
      case mri::ImageFrame::PNG:
        return ImageFormat::kPng;
      case mri::ImageFrame::JPEG:
        return ImageFormat::kJpeg;
      case mri::ImageFrame::FORMAT_UNSPECIFIED:
        return ImageFormat::kNone;
    }
    NOTREACHED_IN_MIGRATION()
        << "Unknown image format: " << image_frame.format();
  }
  return ImageFormat::kNone;
}

ImageFrame ImageFrameProtoToIdl(const mri::ImageFrame& image_frame) {
  ImageFrame image_frame_result;
  if (image_frame.has_width())
    image_frame_result.width = image_frame.width();

  if (image_frame.has_height())
    image_frame_result.height = image_frame.height();

  if (image_frame.has_data_length()) {
    image_frame_result.data_length = image_frame.data_length();
  }

  if (image_frame.has_pixel_data()) {
    image_frame_result.frame.emplace(image_frame.pixel_data().begin(),
                                     image_frame.pixel_data().end());
  }

  image_frame_result.format = ImageFormatProtoToIdl(image_frame);
  return image_frame_result;
}

PerceptionSample PerceptionSampleProtoToIdl(
    const mri::PerceptionSample& perception_sample) {
  PerceptionSample perception_sample_result;
  if (perception_sample.has_frame_perception()) {
    perception_sample_result.frame_perception =
        FramePerceptionProtoToIdl(perception_sample.frame_perception());
  }
  if (perception_sample.has_image_frame()) {
    perception_sample_result.image_frame =
        ImageFrameProtoToIdl(perception_sample.image_frame());
  }
  if (perception_sample.has_audio_perception()) {
    perception_sample_result.audio_perception =
        AudioPerceptionProtoToIdl(perception_sample.audio_perception());
  }
  if (perception_sample.has_audio_visual_perception()) {
    perception_sample_result.audio_visual_perception =
        AudioVisualPerceptionProtoToIdl(
            perception_sample.audio_visual_perception());
  }
  if (perception_sample.has_metadata()) {
    perception_sample_result.metadata =
        MetadataProtoToIdl(perception_sample.metadata());
  }
  return perception_sample_result;
}

Status StateStatusProtoToIdl(const mri::State& state) {
  switch (state.status()) {
    case mri::State::UNINITIALIZED:
      return Status::kUninitialized;
    case mri::State::STARTED:
      return Status::kStarted;
    case mri::State::RUNNING:
      return Status::kRunning;
    case mri::State::SUSPENDED:
      return Status::kSuspended;
    case mri::State::RESTARTING:
      return Status::kRestarting;
    case mri::State::STOPPED:
      return Status::kStopped;
    case mri::State::STATUS_UNSPECIFIED:
      return Status::kNone;
  }
  NOTREACHED_IN_MIGRATION() << "Reached status not in switch.";
  return Status::kNone;
}

mri::State::Status StateStatusIdlToProto(const State& state) {
  switch (state.status) {
    case Status::kUninitialized:
      return mri::State::UNINITIALIZED;
    case Status::kStarted:
      return mri::State::STARTED;
    case Status::kRunning:
      return mri::State::RUNNING;
    case Status::kSuspended:
      return mri::State::SUSPENDED;
    case Status::kRestarting:
      return mri::State::RESTARTING;
    case Status::kStopped:  // Process is stopped by MPP.
      return mri::State::STOPPED;
    case Status::kServiceError:
    case Status::kNone:
      return mri::State::STATUS_UNSPECIFIED;
  }
  NOTREACHED_IN_MIGRATION() << "Reached status not in switch.";
  return mri::State::STATUS_UNSPECIFIED;
}

Feature FeatureProtoToIdl(int feature) {
  switch (feature) {
    case mri::State::FEATURE_AUTOZOOM:
      return Feature::kAutozoom;
    case mri::State::FEATURE_HOTWORD_DETECTION:
      return Feature::kHotwordDetection;
    case mri::State::FEATURE_OCCUPANCY_DETECTION:
      return Feature::kOccupancyDetection;
    case mri::State::FEATURE_EDGE_EMBEDDINGS:
      return Feature::kEdgeEmbeddings;
    case mri::State::FEATURE_SOFTWARE_CROPPING:
      return Feature::kSoftwareCropping;
    case mri::State::FEATURE_UNSET:
      return Feature::kNone;
  }
  NOTREACHED_IN_MIGRATION() << "Reached feature not in switch.";
  return Feature::kNone;
}

mri::State::Feature FeatureIdlToProto(const Feature& feature) {
  switch (feature) {
    case Feature::kAutozoom:
      return mri::State::FEATURE_AUTOZOOM;
    case Feature::kHotwordDetection:
      return mri::State::FEATURE_HOTWORD_DETECTION;
    case Feature::kOccupancyDetection:
      return mri::State::FEATURE_OCCUPANCY_DETECTION;
    case Feature::kEdgeEmbeddings:
      return mri::State::FEATURE_EDGE_EMBEDDINGS;
    case Feature::kSoftwareCropping:
      return mri::State::FEATURE_SOFTWARE_CROPPING;
    case Feature::kNone:
      return mri::State::FEATURE_UNSET;
  }
  NOTREACHED_IN_MIGRATION() << "Reached feature not in switch.";
  return mri::State::FEATURE_UNSET;
}

base::Value NamedTemplateArgumentValueProtoToValue(
    const mri::State::NamedTemplateArgument& named_template_argument) {
  switch (named_template_argument.value_case()) {
    case mri::State::NamedTemplateArgument::ValueCase::kStr:
      return base::Value(named_template_argument.str());
    case mri::State::NamedTemplateArgument::ValueCase::kNum:
      return base::Value(named_template_argument.num());
    case mri::State::NamedTemplateArgument::ValueCase::VALUE_NOT_SET:
      return base::Value();
  }
  NOTREACHED_IN_MIGRATION() << "Unknown NamedTemplateArgument::ValueCase "
                            << named_template_argument.value_case();
  return base::Value();
}

bool NamedTemplateArgumentProtoToIdl(
    const mri::State::NamedTemplateArgument named_template_argument_proto,
    NamedTemplateArgument* named_template_argument) {
  named_template_argument->name = named_template_argument_proto.name();

  base::Value value =
      NamedTemplateArgumentValueProtoToValue(named_template_argument_proto);

  named_template_argument->value =
      NamedTemplateArgument::Value::FromValue(value);
  if (!named_template_argument->value) {
    return false;
  }

  return true;
}

mri::State::NamedTemplateArgument NamedTemplateArgumentIdlToProto(
    const NamedTemplateArgument& named_template_argument) {
  mri::State::NamedTemplateArgument named_template_argument_proto;

  if (named_template_argument.name)
    named_template_argument_proto.set_name(*named_template_argument.name);

  if (named_template_argument.value) {
    if (named_template_argument.value->as_string) {
      named_template_argument_proto.set_str(
          *named_template_argument.value->as_string);
    } else if (named_template_argument.value->as_number) {
      named_template_argument_proto.set_num(
          *named_template_argument.value->as_number);
    } else {
      NOTREACHED_IN_MIGRATION()
          << "Failed to convert NamedTemplateARgument::Value IDL to "
             "Proto, unkown value type.";
    }
  }

  return named_template_argument_proto;
}

void VideoStreamParamIdlToProto(mri::VideoStreamParam* param_result,
                                const VideoStreamParam& param) {
  if (param_result == nullptr)
    return;

  if (param.id)
    param_result->set_id(*param.id);

  if (param.width)
    param_result->set_width(*param.width);

  if (param.height)
    param_result->set_height(*param.height);

  if (param.frame_rate)
    param_result->set_frame_rate(*param.frame_rate);
}

}  //  namespace

Whiteboard WhiteboardProtoToIdl(const mri::Whiteboard& whiteboard) {
  Whiteboard whiteboard_result;
  if (whiteboard.has_top_left())
    whiteboard_result.top_left = PointProtoToIdl(whiteboard.top_left());

  if (whiteboard.has_top_right())
    whiteboard_result.top_right = PointProtoToIdl(whiteboard.top_right());

  if (whiteboard.has_bottom_left())
    whiteboard_result.bottom_left = PointProtoToIdl(whiteboard.bottom_left());

  if (whiteboard.has_bottom_right()) {
    whiteboard_result.bottom_right = PointProtoToIdl(whiteboard.bottom_right());
  }

  if (whiteboard.has_aspect_ratio()) {
    whiteboard_result.aspect_ratio = whiteboard.aspect_ratio();
  }

  return whiteboard_result;
}

void WhiteboardIdlToProto(const Whiteboard& whiteboard,
                          mri::Whiteboard *whiteboard_result) {
  if (whiteboard.top_left) {
    PointIdlToProto(*whiteboard.top_left,
                    whiteboard_result->mutable_top_left());
  }

  if (whiteboard.top_right) {
    PointIdlToProto(*whiteboard.top_right,
                    whiteboard_result->mutable_top_right());
  }

  if (whiteboard.bottom_left) {
    PointIdlToProto(*whiteboard.bottom_left,
                    whiteboard_result->mutable_bottom_left());
  }

  if (whiteboard.bottom_right) {
    PointIdlToProto(*whiteboard.bottom_right,
                    whiteboard_result->mutable_bottom_right());
  }

  if (whiteboard.aspect_ratio)
    whiteboard_result->set_aspect_ratio(*whiteboard.aspect_ratio);
}

State StateProtoToIdl(const mri::State& state) {
  State state_result;
  if (state.has_status()) {
    state_result.status = StateStatusProtoToIdl(state);
  }
  if (state.has_device_context()) {
    state_result.device_context = state.device_context();
  }
  if (state.has_configuration()) {
    state_result.configuration = state.configuration();
  }
  if (state.has_whiteboard())
    state_result.whiteboard = WhiteboardProtoToIdl(state.whiteboard());
  if (state.features_size() > 0) {
    state_result.features.emplace();
    for (const auto& feature : state.features()) {
      const Feature feature_result = FeatureProtoToIdl(feature);
      if (feature_result != Feature::kNone) {
        state_result.features->emplace_back(feature_result);
      }
    }
  }

  if (state.named_template_arguments_size() > 0) {
    state_result.named_template_arguments = std::vector<NamedTemplateArgument>(
        state.named_template_arguments_size());

    for (int i = 0; i < state.named_template_arguments_size(); ++i) {
      const mri::State::NamedTemplateArgument& named_template_argument_proto =
          state.named_template_arguments(i);

      NamedTemplateArgumentProtoToIdl(
          named_template_argument_proto,
          &state_result.named_template_arguments->at(i));
    }
  }

  return state_result;
}

mri::State StateIdlToProto(const State& state) {
  mri::State state_result;
  state_result.set_status(StateStatusIdlToProto(state));
  if (state.device_context)
    state_result.set_device_context(*state.device_context);

  if (state.configuration)
    state_result.set_configuration(*state.configuration);

  if (state.video_stream_param) {
    for (const auto& param : *state.video_stream_param) {
      mri::VideoStreamParam* video_stream_param_result =
          state_result.add_video_stream_param();
      VideoStreamParamIdlToProto(video_stream_param_result, param);
    }
  }

  if (state.whiteboard)
    WhiteboardIdlToProto(*state.whiteboard, state_result.mutable_whiteboard());

  if (state.features) {
    for (const auto& feature : *state.features)
      state_result.add_features(FeatureIdlToProto(feature));
  }

  if (state.named_template_arguments) {
    for (const NamedTemplateArgument& named_template_argument_idl :
         *state.named_template_arguments) {
      mri::State::NamedTemplateArgument* new_named_template_argument_proto =
          state_result.add_named_template_arguments();

      *new_named_template_argument_proto =
          NamedTemplateArgumentIdlToProto(named_template_argument_idl);
    }
  }

  return state_result;
}

MediaPerception MediaPerceptionProtoToIdl(
    const mri::MediaPerception& media_perception) {
  MediaPerception media_perception_result;
  if (media_perception.has_timestamp()) {
    media_perception_result.timestamp = media_perception.timestamp();
  }

  if (media_perception.frame_perception_size() > 0) {
    media_perception_result.frame_perceptions.emplace();
    for (const auto& frame_perception : media_perception.frame_perception()) {
      media_perception_result.frame_perceptions->emplace_back(
          FramePerceptionProtoToIdl(frame_perception));
    }
  }

  if (media_perception.audio_perception_size() > 0) {
    media_perception_result.audio_perceptions.emplace();
    for (const auto& audio_perception : media_perception.audio_perception()) {
      media_perception_result.audio_perceptions->emplace_back(
          AudioPerceptionProtoToIdl(audio_perception));
    }
  }

  if (media_perception.audio_visual_perception_size() > 0) {
    media_perception_result.audio_visual_perceptions.emplace();
    for (const auto& perception : media_perception.audio_visual_perception()) {
      media_perception_result.audio_visual_perceptions->emplace_back(
          AudioVisualPerceptionProtoToIdl(perception));
    }
  }

  if (media_perception.has_metadata()) {
    media_perception_result.metadata =
        MetadataProtoToIdl(media_perception.metadata());
  }

  return media_perception_result;
}

Diagnostics DiagnosticsProtoToIdl(const mri::Diagnostics& diagnostics) {
  Diagnostics diagnostics_result;
  if (diagnostics.perception_sample_size() > 0) {
    diagnostics_result.perception_samples.emplace();
    for (const auto& perception_sample : diagnostics.perception_sample()) {
      diagnostics_result.perception_samples->emplace_back(
          PerceptionSampleProtoToIdl(perception_sample));
    }
  }
  return diagnostics_result;
}

}  // namespace media_perception_private
}  // namespace api
}  // namespace extensions
