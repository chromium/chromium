// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/conversion_utils.h"
#include "base/notreached.h"

namespace extensions {
namespace api {
namespace media_perception_private {

namespace {

std::unique_ptr<Metadata> MetadataProtoToIdl(const mri::Metadata& metadata) {
  std::unique_ptr<Metadata> metadata_result = std::make_unique<Metadata>();
  if (metadata.has_visual_experience_controller_version()) {
    metadata_result->visual_experience_controller_version =
        std::make_unique<std::string>(
            metadata.visual_experience_controller_version());
  }

  return metadata_result;
}

HotwordType HotwordTypeProtoToIdl(const mri::HotwordDetection::Type& type) {
  switch (type) {
    case mri::HotwordDetection::UNKNOWN_TYPE:
      return HOTWORD_TYPE_UNKNOWN_TYPE;
    case mri::HotwordDetection::OK_GOOGLE:
      return HOTWORD_TYPE_OK_GOOGLE;
  }
  NOTREACHED() << "Unknown hotword type: " << type;
  return HOTWORD_TYPE_UNKNOWN_TYPE;
}

Hotword HotwordProtoToIdl(const mri::HotwordDetection::Hotword& hotword) {
  Hotword hotword_result;
  if (hotword.has_id())
    hotword_result.id = std::make_unique<int>(hotword.id());

  if (hotword.has_type())
    hotword_result.type = HotwordTypeProtoToIdl(hotword.type());

  if (hotword.has_frame_id())
    hotword_result.frame_id = std::make_unique<int>(hotword.frame_id());

  if (hotword.has_start_timestamp_ms()) {
    hotword_result.start_timestamp_ms =
        std::make_unique<int>(hotword.start_timestamp_ms());
  }

  if (hotword.has_end_timestamp_ms()) {
    hotword_result.end_timestamp_ms =
        std::make_unique<int>(hotword.end_timestamp_ms());
  }

  if (hotword.has_confidence())
    hotword_result.confidence = std::make_unique<double>(hotword.confidence());

  if (hotword.has_id())
    hotword_result.id = std::make_unique<int>(hotword.id());

  return hotword_result;
}

std::unique_ptr<HotwordDetection> HotwordDetectionProtoToIdl(
    const mri::HotwordDetection& detection) {
  std::unique_ptr<HotwordDetection> detection_result =
      std::make_unique<HotwordDetection>();

  if (detection.hotwords_size() > 0) {
    detection_result->hotwords = std::make_unique<std::vector<Hotword>>();
    for (const auto& hotword : detection.hotwords()) {
      detection_result->hotwords->emplace_back(HotwordProtoToIdl(hotword));
    }
  }

  return detection_result;
}

std::unique_ptr<AudioSpectrogram> AudioSpectrogramProtoToIdl(
    const mri::AudioSpectrogram& spectrogram) {
  std::unique_ptr<AudioSpectrogram> spectrogram_result =
      std::make_unique<AudioSpectrogram>();
  if (spectrogram.values_size() > 0) {
    spectrogram_result->values = std::make_unique<std::vector<double>>();
    for (const auto& value : spectrogram.values()) {
      spectrogram_result->values->emplace_back(value);
    }
  }
  return spectrogram_result;
}

std::unique_ptr<AudioHumanPresenceDetection>
AudioHumanPresenceDetectionProtoToIdl(
    const mri::AudioHumanPresenceDetection& detection) {
  std::unique_ptr<AudioHumanPresenceDetection> detection_result =
      std::make_unique<AudioHumanPresenceDetection>();
  if (detection.has_human_presence_likelihood()) {
    detection_result->human_presence_likelihood =
        std::make_unique<double>(detection.human_presence_likelihood());
  }
  if (detection.has_noise_spectrogram()) {
    detection_result->noise_spectrogram =
        AudioSpectrogramProtoToIdl(detection.noise_spectrogram());
  }
  if (detection.has_frame_spectrogram()) {
    detection_result->frame_spectrogram =
        AudioSpectrogramProtoToIdl(detection.frame_spectrogram());
  }
  return detection_result;
}

std::unique_ptr<AudioLocalization> AudioLocalizationProtoToIdl(
    const mri::AudioLocalization& localization) {
  std::unique_ptr<AudioLocalization> localization_result =
      std::make_unique<AudioLocalization>();
  if (localization.has_azimuth_radians()) {
    localization_result->azimuth_radians =
        std::make_unique<double>(localization.azimuth_radians());
  }
  if (localization.azimuth_scores_size() > 0) {
    localization_result->azimuth_scores =
        std::make_unique<std::vector<double>>();
    for (const auto& score : localization.azimuth_scores()) {
      localization_result->azimuth_scores->emplace_back(score);
    }
  }
  return localization_result;
}

AudioPerception AudioPerceptionProtoToIdl(
    const mri::AudioPerception& perception) {
  AudioPerception perception_result;
  if (perception.has_timestamp_us()) {
    perception_result.timestamp_us =
        std::make_unique<double>(perception.timestamp_us());
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
      return LIGHT_CONDITION_UNSPECIFIED;
    case mri::VideoHumanPresenceDetection::NO_CHANGE:
      return LIGHT_CONDITION_NO_CHANGE;
    case mri::VideoHumanPresenceDetection::TURNED_ON:
      return LIGHT_CONDITION_TURNED_ON;
    case mri::VideoHumanPresenceDetection::TURNED_OFF:
      return LIGHT_CONDITION_TURNED_OFF;
    case mri::VideoHumanPresenceDetection::DIMMER:
      return LIGHT_CONDITION_DIMMER;
    case mri::VideoHumanPresenceDetection::BRIGHTER:
      return LIGHT_CONDITION_BRIGHTER;
    case mri::VideoHumanPresenceDetection::BLACK_FRAME:
      return LIGHT_CONDITION_BLACK_FRAME;
    default:
      NOTREACHED() << "Unknown light condition: " << condition;
      return LIGHT_CONDITION_UNSPECIFIED;
  }
}

std::unique_ptr<VideoHumanPresenceDetection>
VideoHumanPresenceDetectionProtoToIdl(
    const mri::VideoHumanPresenceDetection& detection) {
  std::unique_ptr<VideoHumanPresenceDetection> detection_result =
      std::make_unique<VideoHumanPresenceDetection>();
  if (detection.has_human_presence_likelihood()) {
    detection_result->human_presence_likelihood =
        std::make_unique<double>(detection.human_presence_likelihood());
  }

  if (detection.has_motion_detected_likelihood()) {
    detection_result->motion_detected_likelihood =
        std::make_unique<double>(detection.motion_detected_likelihood());
  }

  if (detection.has_light_condition()) {
    detection_result->light_condition =
        LightConditionProtoToIdl(detection.light_condition());
  }

  if (detection.has_light_condition_likelihood()) {
    detection_result->light_condition_likelihood =
        std::make_unique<double>(detection.light_condition_likelihood());
  }

  return detection_result;
}

std::unique_ptr<AudioVisualHumanPresenceDetection>
AudioVisualHumanPresenceDetectionProtoToIdl(
    const mri::AudioVisualHumanPresenceDetection& detection) {
  std::unique_ptr<AudioVisualHumanPresenceDetection> detection_result =
      std::make_unique<AudioVisualHumanPresenceDetection>();

  if (detection.has_human_presence_likelihood()) {
    detection_result->human_presence_likelihood =
        std::make_unique<double>(detection.human_presence_likelihood());
  }

  return detection_result;
}

AudioVisualPerception AudioVisualPerceptionProtoToIdl(
    const mri::AudioVisualPerception& perception) {
  AudioVisualPerception perception_result;
  if (perception.has_timestamp_us()) {
    perception_result.timestamp_us =
        std::make_unique<double>(perception.timestamp_us());
  }
  if (perception.has_audio_visual_human_presence_detection()) {
    perception_result.audio_visual_human_presence_detection =
        AudioVisualHumanPresenceDetectionProtoToIdl(
            perception.audio_visual_human_presence_detection());
  }
  return perception_result;
}

std::unique_ptr<Point> PointProtoToIdl(const mri::Point& point) {
  std::unique_ptr<Point> point_result = std::make_unique<Point>();
  if (point.has_x())
    point_result->x = std::make_unique<double>(point.x());

  if (point.has_y())
    point_result->y = std::make_unique<double>(point.y());

  return point_result;
}

void PointIdlToProto(const Point& point, mri::Point* point_result) {
  if (point.x)
    point_result->set_x(*point.x);

  if (point.y)
    point_result->set_y(*point.y);
}

std::unique_ptr<BoundingBox> BoundingBoxProtoToIdl(
    const mri::BoundingBox& bounding_box) {
  std::unique_ptr<BoundingBox> bounding_box_result =
      std::make_unique<BoundingBox>();
  if (bounding_box.has_normalized()) {
    bounding_box_result->normalized =
        std::make_unique<bool>(bounding_box.normalized());
  }

  if (bounding_box.has_top_left())
    bounding_box_result->top_left = PointProtoToIdl(bounding_box.top_left());

  if (bounding_box.has_bottom_right()) {
    bounding_box_result->bottom_right =
        PointProtoToIdl(bounding_box.bottom_right());
  }

  return bounding_box_result;
}

DistanceUnits DistanceUnitsProtoToIdl(const mri::Distance& distance) {
  if (distance.has_units()) {
    switch (distance.units()) {
      case mri::Distance::METERS:
        return DISTANCE_UNITS_METERS;
      case mri::Distance::PIXELS:
        return DISTANCE_UNITS_PIXELS;
      case mri::Distance::UNITS_UNSPECIFIED:
        return DISTANCE_UNITS_UNSPECIFIED;
    }
    NOTREACHED() << "Unknown distance units: " << distance.units();
  }
  return DISTANCE_UNITS_UNSPECIFIED;
}

std::unique_ptr<Distance> DistanceProtoToIdl(const mri::Distance& distance) {
  std::unique_ptr<Distance> distance_result = std::make_unique<Distance>();
  distance_result->units = DistanceUnitsProtoToIdl(distance);

  if (distance.has_magnitude())
    distance_result->magnitude = std::make_unique<double>(distance.magnitude());

  return distance_result;
}

FramePerceptionType FramePerceptionTypeProtoToIdl(int type) {
  switch (type) {
    case mri::FramePerception::UNKNOWN_TYPE:
      return FRAME_PERCEPTION_TYPE_UNKNOWN_TYPE;
    case mri::FramePerception::FACE_DETECTION:
      return FRAME_PERCEPTION_TYPE_FACE_DETECTION;
    case mri::FramePerception::PERSON_DETECTION:
      return FRAME_PERCEPTION_TYPE_PERSON_DETECTION;
    case mri::FramePerception::MOTION_DETECTION:
      return FRAME_PERCEPTION_TYPE_MOTION_DETECTION;
  }
  NOTREACHED() << "Unknown frame perception type: " << type;
  return FRAME_PERCEPTION_TYPE_UNKNOWN_TYPE;
}

EntityType EntityTypeProtoToIdl(const mri::Entity& entity) {
  if (entity.has_type()) {
    switch (entity.type()) {
      case mri::Entity::FACE:
        return ENTITY_TYPE_FACE;
      case mri::Entity::PERSON:
        return ENTITY_TYPE_PERSON;
      case mri::Entity::MOTION_REGION:
        return ENTITY_TYPE_MOTION_REGION;
      case mri::Entity::LABELED_REGION:
        return ENTITY_TYPE_LABELED_REGION;
      case mri::Entity::UNSPECIFIED:
        return ENTITY_TYPE_UNSPECIFIED;
    }
    NOTREACHED() << "Unknown entity type: " << entity.type();
  }
  return ENTITY_TYPE_UNSPECIFIED;
}

Entity EntityProtoToIdl(const mri::Entity& entity) {
  Entity entity_result;
  if (entity.has_id())
    entity_result.id = std::make_unique<int>(entity.id());

  entity_result.type = EntityTypeProtoToIdl(entity);
  if (entity.has_confidence())
    entity_result.confidence = std::make_unique<double>(entity.confidence());

  if (entity.has_bounding_box())
    entity_result.bounding_box = BoundingBoxProtoToIdl(entity.bounding_box());

  if (entity.has_depth())
    entity_result.depth = DistanceProtoToIdl(entity.depth());

  if (entity.has_label())
    entity_result.entity_label = std::make_unique<std::string>(entity.label());

  return entity_result;
}

PacketLatency PacketLatencyProtoToIdl(
    const mri::PacketLatency& packet_latency) {
  PacketLatency packet_latency_result;

  if (packet_latency.has_label()) {
    packet_latency_result.packet_label =
        std::make_unique<std::string>(packet_latency.label());
  }

  if (packet_latency.has_latency_usec()) {
    packet_latency_result.latency_usec =
        std::make_unique<int>(packet_latency.latency_usec());
  }

  return packet_latency_result;
}

FramePerception FramePerceptionProtoToIdl(
    const mri::FramePerception& frame_perception) {
  FramePerception frame_perception_result;
  if (frame_perception.has_frame_id()) {
    frame_perception_result.frame_id =
        std::make_unique<int>(frame_perception.frame_id());
  }
  if (frame_perception.has_frame_width_in_px()) {
    frame_perception_result.frame_width_in_px =
        std::make_unique<int>(frame_perception.frame_width_in_px());
  }
  if (frame_perception.has_frame_height_in_px()) {
    frame_perception_result.frame_height_in_px =
        std::make_unique<int>(frame_perception.frame_height_in_px());
  }
  if (frame_perception.has_timestamp()) {
    frame_perception_result.timestamp =
        std::make_unique<double>(frame_perception.timestamp());
  }
  if (frame_perception.entity_size() > 0) {
    frame_perception_result.entities = std::make_unique<std::vector<Entity>>();
    for (const auto& entity : frame_perception.entity())
      frame_perception_result.entities->emplace_back(EntityProtoToIdl(entity));
  }
  if (frame_perception.packet_latency_size() > 0) {
    frame_perception_result.packet_latency =
        std::make_unique<std::vector<PacketLatency>>();
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
    frame_perception_result.frame_perception_types =
        std::make_unique<std::vector<FramePerceptionType>>();
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
        return IMAGE_FORMAT_RAW;
      case mri::ImageFrame::PNG:
        return IMAGE_FORMAT_PNG;
      case mri::ImageFrame::JPEG:
        return IMAGE_FORMAT_JPEG;
      case mri::ImageFrame::FORMAT_UNSPECIFIED:
        return IMAGE_FORMAT_NONE;
    }
    NOTREACHED() << "Unknown image format: " << image_frame.format();
  }
  return IMAGE_FORMAT_NONE;
}

ImageFrame ImageFrameProtoToIdl(const mri::ImageFrame& image_frame) {
  ImageFrame image_frame_result;
  if (image_frame.has_width())
    image_frame_result.width = std::make_unique<int>(image_frame.width());

  if (image_frame.has_height())
    image_frame_result.height = std::make_unique<int>(image_frame.height());

  if (image_frame.has_data_length()) {
    image_frame_result.data_length =
        std::make_unique<int>(image_frame.data_length());
  }

  if (image_frame.has_pixel_data()) {
    image_frame_result.frame = std::make_unique<std::vector<uint8_t>>(
        image_frame.pixel_data().begin(), image_frame.pixel_data().end());
  }

  image_frame_result.format = ImageFormatProtoToIdl(image_frame);
  return image_frame_result;
}

PerceptionSample PerceptionSampleProtoToIdl(
    const mri::PerceptionSample& perception_sample) {
  PerceptionSample perception_sample_result;
  if (perception_sample.has_frame_perception()) {
    perception_sample_result.frame_perception =
        std::make_unique<FramePerception>(
            FramePerceptionProtoToIdl(perception_sample.frame_perception()));
  }
  if (perception_sample.has_image_frame()) {
    perception_sample_result.image_frame = std::make_unique<ImageFrame>(
        ImageFrameProtoToIdl(perception_sample.image_frame()));
  }
  if (perception_sample.has_audio_perception()) {
    perception_sample_result.audio_perception =
        std::make_unique<AudioPerception>(
            AudioPerceptionProtoToIdl(perception_sample.audio_perception()));
  }
  if (perception_sample.has_audio_visual_perception()) {
    perception_sample_result.audio_visual_perception =
        std::make_unique<AudioVisualPerception>(AudioVisualPerceptionProtoToIdl(
            perception_sample.audio_visual_perception()));
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
      return STATUS_UNINITIALIZED;
    case mri::State::STARTED:
      return STATUS_STARTED;
    case mri::State::RUNNING:
      return STATUS_RUNNING;
    case mri::State::SUSPENDED:
      return STATUS_SUSPENDED;
    case mri::State::RESTARTING:
      return STATUS_RESTARTING;
    case mri::State::STOPPED:
      return STATUS_STOPPED;
    case mri::State::STATUS_UNSPECIFIED:
      return STATUS_NONE;
  }
  NOTREACHED() << "Reached status not in switch.";
  return STATUS_NONE;
}

mri::State::Status StateStatusIdlToProto(const State& state) {
  switch (state.status) {
    case STATUS_UNINITIALIZED:
      return mri::State::UNINITIALIZED;
    case STATUS_STARTED:
      return mri::State::STARTED;
    case STATUS_RUNNING:
      return mri::State::RUNNING;
    case STATUS_SUSPENDED:
      return mri::State::SUSPENDED;
    case STATUS_RESTARTING:
      return mri::State::RESTARTING;
    case STATUS_STOPPED:  // Process is stopped by MPP.
      return mri::State::STOPPED;
    case STATUS_SERVICE_ERROR:
    case STATUS_NONE:
      return mri::State::STATUS_UNSPECIFIED;
  }
  NOTREACHED() << "Reached status not in switch.";
  return mri::State::STATUS_UNSPECIFIED;
}

Feature FeatureProtoToIdl(int feature) {
  switch (feature) {
    case mri::State::FEATURE_AUTOZOOM:
      return FEATURE_AUTOZOOM;
    case mri::State::FEATURE_HOTWORD_DETECTION:
      return FEATURE_HOTWORD_DETECTION;
    case mri::State::FEATURE_OCCUPANCY_DETECTION:
      return FEATURE_OCCUPANCY_DETECTION;
    case mri::State::FEATURE_EDGE_EMBEDDINGS:
      return FEATURE_EDGE_EMBEDDINGS;
    case mri::State::FEATURE_SOFTWARE_CROPPING:
      return FEATURE_SOFTWARE_CROPPING;
    case mri::State::FEATURE_UNSET:
      return FEATURE_NONE;
  }
  NOTREACHED() << "Reached feature not in switch.";
  return FEATURE_NONE;
}

mri::State::Feature FeatureIdlToProto(const Feature& feature) {
  switch (feature) {
    case FEATURE_AUTOZOOM:
      return mri::State::FEATURE_AUTOZOOM;
    case FEATURE_HOTWORD_DETECTION:
      return mri::State::FEATURE_HOTWORD_DETECTION;
    case FEATURE_OCCUPANCY_DETECTION:
      return mri::State::FEATURE_OCCUPANCY_DETECTION;
    case FEATURE_EDGE_EMBEDDINGS:
      return mri::State::FEATURE_EDGE_EMBEDDINGS;
    case FEATURE_SOFTWARE_CROPPING:
      return mri::State::FEATURE_SOFTWARE_CROPPING;
    case FEATURE_NONE:
      return mri::State::FEATURE_UNSET;
  }
  NOTREACHED() << "Reached feature not in switch.";
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
  NOTREACHED() << "Unknown NamedTemplateArgument::ValueCase "
               << named_template_argument.value_case();
  return base::Value();
}

bool NamedTemplateArgumentProtoToIdl(
    const mri::State::NamedTemplateArgument named_template_argument_proto,
    NamedTemplateArgument* named_template_argument) {
  named_template_argument->name =
      std::make_unique<std::string>(named_template_argument_proto.name());

  base::Value value =
      NamedTemplateArgumentValueProtoToValue(named_template_argument_proto);

  named_template_argument->value =
      std::make_unique<NamedTemplateArgument::Value>();
  if (!NamedTemplateArgument::Value::Populate(
          value, named_template_argument->value.get())) {
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
      NOTREACHED() << "Failed to convert NamedTemplateARgument::Value IDL to "
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

std::unique_ptr<Whiteboard> WhiteboardProtoToIdl(
    const mri::Whiteboard& whiteboard) {
  std::unique_ptr<Whiteboard> whiteboard_result =
      std::make_unique<Whiteboard>();
  if (whiteboard.has_top_left())
    whiteboard_result->top_left = PointProtoToIdl(whiteboard.top_left());

  if (whiteboard.has_top_right())
    whiteboard_result->top_right = PointProtoToIdl(whiteboard.top_right());

  if (whiteboard.has_bottom_left())
    whiteboard_result->bottom_left = PointProtoToIdl(whiteboard.bottom_left());

  if (whiteboard.has_bottom_right()) {
    whiteboard_result->bottom_right =
        PointProtoToIdl(whiteboard.bottom_right());
  }

  if (whiteboard.has_aspect_ratio()) {
    whiteboard_result->aspect_ratio =
        std::make_unique<double>(whiteboard.aspect_ratio());
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
    state_result.device_context =
        std::make_unique<std::string>(state.device_context());
  }
  if (state.has_configuration()) {
    state_result.configuration =
        std::make_unique<std::string>(state.configuration());
  }
  if (state.has_whiteboard())
    state_result.whiteboard = WhiteboardProtoToIdl(state.whiteboard());
  if (state.features_size() > 0) {
    state_result.features = std::make_unique<std::vector<Feature>>();
    for (const auto& feature : state.features()) {
      const Feature feature_result = FeatureProtoToIdl(feature);
      if (feature_result != FEATURE_NONE)
        state_result.features->emplace_back(feature_result);
    }
  }

  if (state.named_template_arguments_size() > 0) {
    state_result.named_template_arguments =
        std::make_unique<std::vector<NamedTemplateArgument>>(
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

  if (state.video_stream_param && state.video_stream_param.get() != nullptr) {
    for (size_t i = 0; i < state.video_stream_param.get()->size(); ++i) {
      mri::VideoStreamParam* video_stream_param_result =
          state_result.add_video_stream_param();
      VideoStreamParamIdlToProto(video_stream_param_result,
                                 state.video_stream_param.get()->at(i));
    }
  }

  if (state.whiteboard)
    WhiteboardIdlToProto(*state.whiteboard, state_result.mutable_whiteboard());

  if (state.features && state.features.get() != nullptr) {
    for (size_t i = 0; i < state.features.get()->size(); ++i)
      state_result.add_features(FeatureIdlToProto(state.features.get()->at(i)));
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
    media_perception_result.timestamp =
        std::make_unique<double>(media_perception.timestamp());
  }

  if (media_perception.frame_perception_size() > 0) {
    media_perception_result.frame_perceptions =
        std::make_unique<std::vector<FramePerception>>();
    for (const auto& frame_perception : media_perception.frame_perception()) {
      media_perception_result.frame_perceptions->emplace_back(
          FramePerceptionProtoToIdl(frame_perception));
    }
  }

  if (media_perception.audio_perception_size() > 0) {
    media_perception_result.audio_perceptions =
        std::make_unique<std::vector<AudioPerception>>();
    for (const auto& audio_perception : media_perception.audio_perception()) {
      media_perception_result.audio_perceptions->emplace_back(
          AudioPerceptionProtoToIdl(audio_perception));
    }
  }

  if (media_perception.audio_visual_perception_size() > 0) {
    media_perception_result.audio_visual_perceptions =
        std::make_unique<std::vector<AudioVisualPerception>>();
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
    diagnostics_result.perception_samples =
        std::make_unique<std::vector<PerceptionSample>>();
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
