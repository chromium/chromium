// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/conversion_utils.h"

#include <memory>

#include "chromeos/ash/components/dbus/media_perception/media_perception.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace media_perception = extensions::api::media_perception_private;

namespace extensions {

namespace {

const char kTestDeviceContext[] = "Video camera";
const char kTestConfiguration[] = "dummy_config";
const char kFakePacketLabel1[] = "Packet1";
const char kFakePacketLabel3[] = "Packet3";
const char kFakeEntityLabel3[] = "Region3";
const char kVideoStreamIdForFaceDetection[] = "FaceDetection";
const char kVideoStreamIdForVideoCapture[] = "VideoCapture";

const int kVideoStreamWidthForFaceDetection = 1280;
const int kVideoStreamHeightForFaceDetection = 720;
const int kVideoStreamFrameRateForFaceDetection = 30;
const int kVideoStreamWidthForVideoCapture = 640;
const int kVideoStreamHeightForVideoCapture = 360;
const int kVideoStreamFrameRateForVideoCapture = 5;

constexpr float kWhiteboardTopLeftX = 0.001;
constexpr float kWhiteboardTopLeftY = 0.0015;
constexpr float kWhiteboardTopRightX = 0.9;
constexpr float kWhiteboardTopRightY = 0.002;
constexpr float kWhiteboardBottomLeftX = 0.0018;
constexpr float kWhiteboardBottomLeftY = 0.88;
constexpr float kWhiteboardBottomRightX = 0.85;
constexpr float kWhiteboardBottomRightY = 0.79;
constexpr float kWhiteboardAspectRatio = 1.76;

// One numerical value, one empty and one string NamedTemplateArgument
constexpr unsigned kNamedTemplateArgumentsSize = 3;

constexpr char kNumericalTemplateArgumentName[] = "numerical_template_argument";
constexpr double kNumericalTemplateArgumentValue = 42;
constexpr char kStringTemplateArgumentName[] = "string_template_argument";
constexpr char kStringTemplateArgumentValue[] = "string_value";
constexpr double kDoubleTolerance = 0.01;

void InitializeVideoStreamParam(media_perception::VideoStreamParam& param,
                                const std::string& id,
                                int width,
                                int height,
                                int frame_rate) {
  param.id = id;
  param.width = width;
  param.height = height;
  param.frame_rate = frame_rate;
}

void InitializeFakeMetadata(mri::Metadata* metadata) {
  metadata->set_visual_experience_controller_version("30.0");
}

void InitializeFakeAudioPerception(mri::AudioPerception* audio_perception) {
  audio_perception->set_timestamp_us(10086);

  mri::AudioLocalization* audio_localization =
      audio_perception->mutable_audio_localization();
  audio_localization->set_azimuth_radians(1.5);
  audio_localization->add_azimuth_scores(2.0);
  audio_localization->add_azimuth_scores(5.0);

  mri::AudioHumanPresenceDetection* detection =
      audio_perception->mutable_audio_human_presence_detection();
  detection->set_human_presence_likelihood(0.4);

  mri::HotwordDetection* hotword_detection =
      audio_perception->mutable_hotword_detection();
  mri::HotwordDetection::Hotword* hotword_one =
      hotword_detection->add_hotwords();
  hotword_one->set_type(mri::HotwordDetection::OK_GOOGLE);
  hotword_one->set_frame_id(987);
  hotword_one->set_start_timestamp_ms(10456);
  hotword_one->set_end_timestamp_ms(234567);
  hotword_one->set_confidence(0.9);
  hotword_one->set_id(4567);

  mri::HotwordDetection::Hotword* hotword_two =
      hotword_detection->add_hotwords();
  hotword_two->set_type(mri::HotwordDetection::UNKNOWN_TYPE);
  hotword_two->set_frame_id(789);
  hotword_two->set_start_timestamp_ms(65401);
  hotword_two->set_end_timestamp_ms(765432);
  hotword_two->set_confidence(0.4);
  hotword_two->set_id(7654);

  mri::AudioSpectrogram* noise_spectrogram =
      detection->mutable_noise_spectrogram();
  noise_spectrogram->add_values(0.1);
  noise_spectrogram->add_values(0.2);

  mri::AudioSpectrogram* frame_spectrogram =
      detection->mutable_frame_spectrogram();
  frame_spectrogram->add_values(0.3);
}

void InitializeFakeAudioVisualPerception(
    mri::AudioVisualPerception* audio_visual_perception) {
  audio_visual_perception->set_timestamp_us(91008);

  mri::AudioVisualHumanPresenceDetection* detection =
      audio_visual_perception->mutable_audio_visual_human_presence_detection();
  detection->set_human_presence_likelihood(0.5);
}

void InitializeFakeFramePerception(const int index,
                                   mri::FramePerception* frame_perception) {
  frame_perception->set_frame_id(index);
  frame_perception->set_frame_width_in_px(3);
  frame_perception->set_frame_height_in_px(4);
  frame_perception->set_timestamp(5);

  // Add a couple fake packet latency to the frame perception.
  mri::PacketLatency* packet_latency_one =
      frame_perception->add_packet_latency();
  packet_latency_one->set_label(kFakePacketLabel1);
  packet_latency_one->set_latency_usec(10011);

  mri::PacketLatency* packet_latency_two =
      frame_perception->add_packet_latency();
  packet_latency_two->set_latency_usec(20011);

  mri::PacketLatency* packet_latency_three =
      frame_perception->add_packet_latency();
  packet_latency_three->set_label(kFakePacketLabel3);

  // Add a couple fake entities to the frame perception. Note: PERSON
  // EntityType is currently unused.
  mri::Entity* entity_one = frame_perception->add_entity();
  entity_one->set_id(6);
  entity_one->set_type(mri::Entity::FACE);
  entity_one->set_confidence(7);

  mri::Distance* distance = entity_one->mutable_depth();
  distance->set_units(mri::Distance::METERS);
  distance->set_magnitude(7.5);

  mri::Entity* entity_two = frame_perception->add_entity();
  entity_two->set_id(8);
  entity_two->set_type(mri::Entity::MOTION_REGION);
  entity_two->set_confidence(9);

  mri::BoundingBox* bounding_box_one = entity_one->mutable_bounding_box();
  bounding_box_one->mutable_top_left()->set_x(10);
  bounding_box_one->mutable_top_left()->set_y(11);
  bounding_box_one->mutable_bottom_right()->set_x(12);
  bounding_box_one->mutable_bottom_right()->set_y(13);
  bounding_box_one->set_normalized(false);

  mri::BoundingBox* bounding_box_two = entity_two->mutable_bounding_box();
  bounding_box_two->mutable_top_left()->set_x(14);
  bounding_box_two->mutable_top_left()->set_y(15);
  bounding_box_two->set_normalized(true);

  mri::Entity* entity_three = frame_perception->add_entity();
  entity_three->set_type(mri::Entity::LABELED_REGION);
  entity_three->set_label(kFakeEntityLabel3);

  // Add fake video human presence detection.
  mri::VideoHumanPresenceDetection* detection =
      frame_perception->mutable_video_human_presence_detection();
  detection->set_human_presence_likelihood(0.1);
  detection->set_motion_detected_likelihood(0.2);
  detection->set_light_condition(mri::VideoHumanPresenceDetection::BLACK_FRAME);
  detection->set_light_condition_likelihood(0.3);

  // Add fake frame perception types.
  frame_perception->add_perception_types(mri::FramePerception::FACE_DETECTION);
  frame_perception->add_perception_types(
      mri::FramePerception::PERSON_DETECTION);
  frame_perception->add_perception_types(
      mri::FramePerception::MOTION_DETECTION);
}

media_perception::Point MakePointIdl(float x, float y) {
  media_perception::Point point;
  point.x = x;
  point.y = y;
  return point;
}

void ValidateMetadataResult(const media_perception::Metadata& metadata_result) {
  ASSERT_TRUE(metadata_result.visual_experience_controller_version);
  EXPECT_EQ(*metadata_result.visual_experience_controller_version, "30.0");
}

void ValidateFramePerceptionResult(
    const int index,
    const media_perception::FramePerception& frame_perception_result) {
  ASSERT_TRUE(frame_perception_result.frame_id);
  EXPECT_EQ(*frame_perception_result.frame_id, index);
  ASSERT_TRUE(frame_perception_result.frame_width_in_px);
  EXPECT_EQ(*frame_perception_result.frame_width_in_px, 3);
  ASSERT_TRUE(frame_perception_result.frame_height_in_px);
  EXPECT_EQ(*frame_perception_result.frame_height_in_px, 4);
  ASSERT_TRUE(frame_perception_result.timestamp);
  EXPECT_EQ(*frame_perception_result.timestamp, 5);

  // Validate packet latency.
  ASSERT_EQ(3u, frame_perception_result.packet_latency->size());
  const media_perception::PacketLatency& packet_latency_one =
      frame_perception_result.packet_latency->at(0);
  EXPECT_EQ(*packet_latency_one.packet_label, kFakePacketLabel1);
  EXPECT_EQ(*packet_latency_one.latency_usec, 10011);

  const media_perception::PacketLatency& packet_latency_two =
      frame_perception_result.packet_latency->at(1);
  EXPECT_FALSE(packet_latency_two.packet_label);
  EXPECT_EQ(*packet_latency_two.latency_usec, 20011);

  const media_perception::PacketLatency& packet_latency_three =
      frame_perception_result.packet_latency->at(2);
  EXPECT_EQ(*packet_latency_three.packet_label, kFakePacketLabel3);
  EXPECT_FALSE(packet_latency_three.latency_usec);

  // Validate entities.
  ASSERT_EQ(3u, frame_perception_result.entities->size());
  const media_perception::Entity& entity_result_one =
      frame_perception_result.entities->at(0);
  ASSERT_TRUE(entity_result_one.id);
  EXPECT_EQ(*entity_result_one.id, 6);
  ASSERT_TRUE(entity_result_one.confidence);
  EXPECT_EQ(*entity_result_one.confidence, 7);
  EXPECT_EQ(entity_result_one.type, media_perception::EntityType::kFace);

  const std::optional<media_perception::Distance>& distance =
      entity_result_one.depth;
  ASSERT_TRUE(distance);
  EXPECT_EQ(media_perception::DistanceUnits::kMeters, distance->units);
  ASSERT_TRUE(distance->magnitude);
  EXPECT_EQ(7.5f, *distance->magnitude);

  const media_perception::Entity& entity_result_two =
      frame_perception_result.entities->at(1);
  ASSERT_TRUE(entity_result_two.id);
  EXPECT_EQ(*entity_result_two.id, 8);
  ASSERT_TRUE(entity_result_two.confidence);
  EXPECT_EQ(*entity_result_two.confidence, 9);
  EXPECT_EQ(entity_result_two.type,
            media_perception::EntityType::kMotionRegion);

  const std::optional<media_perception::BoundingBox>& bounding_box_result_one =
      entity_result_one.bounding_box;
  ASSERT_TRUE(bounding_box_result_one);
  ASSERT_TRUE(bounding_box_result_one->top_left);
  ASSERT_TRUE(bounding_box_result_one->top_left->x);
  EXPECT_EQ(*bounding_box_result_one->top_left->x, 10);
  ASSERT_TRUE(bounding_box_result_one->top_left->y);
  EXPECT_EQ(*bounding_box_result_one->top_left->y, 11);
  ASSERT_TRUE(bounding_box_result_one->bottom_right);
  ASSERT_TRUE(bounding_box_result_one->bottom_right->x);
  EXPECT_EQ(*bounding_box_result_one->bottom_right->x, 12);
  ASSERT_TRUE(bounding_box_result_one->bottom_right->y);
  EXPECT_EQ(*bounding_box_result_one->bottom_right->y, 13);
  EXPECT_FALSE(*bounding_box_result_one->normalized);

  const std::optional<media_perception::BoundingBox>& bounding_box_result_two =
      entity_result_two.bounding_box;
  ASSERT_TRUE(bounding_box_result_two);
  ASSERT_TRUE(bounding_box_result_two->top_left);
  EXPECT_EQ(*bounding_box_result_two->top_left->x, 14);
  EXPECT_EQ(*bounding_box_result_two->top_left->y, 15);
  EXPECT_FALSE(bounding_box_result_two->bottom_right);
  EXPECT_TRUE(*bounding_box_result_two->normalized);

  const media_perception::Entity& entity_result_three =
      frame_perception_result.entities->at(2);
  ASSERT_TRUE(entity_result_three.entity_label);
  EXPECT_EQ(*entity_result_three.entity_label, kFakeEntityLabel3);
  EXPECT_EQ(entity_result_three.type,
            media_perception::EntityType::kLabeledRegion);

  // Validate video human presence detection.
  const std::optional<media_perception::VideoHumanPresenceDetection>&
      detection_result = frame_perception_result.video_human_presence_detection;
  ASSERT_TRUE(detection_result->human_presence_likelihood);
  EXPECT_EQ(*detection_result->human_presence_likelihood, 0.1);
  ASSERT_TRUE(detection_result->motion_detected_likelihood);
  EXPECT_EQ(*detection_result->motion_detected_likelihood, 0.2);
  EXPECT_EQ(detection_result->light_condition,
            media_perception::LightCondition::kBlackFrame);
  ASSERT_TRUE(detection_result->light_condition_likelihood);
  EXPECT_EQ(*detection_result->light_condition_likelihood, 0.3);

  // Validate frame perception types.
  ASSERT_EQ(3u, frame_perception_result.frame_perception_types->size());
  EXPECT_EQ(frame_perception_result.frame_perception_types->at(0),
            media_perception::FramePerceptionType::kFaceDetection);
  EXPECT_EQ(frame_perception_result.frame_perception_types->at(1),
            media_perception::FramePerceptionType::kPersonDetection);
  EXPECT_EQ(frame_perception_result.frame_perception_types->at(2),
            media_perception::FramePerceptionType::kMotionDetection);
}

void ValidateAudioPerceptionResult(
    const media_perception::AudioPerception& audio_perception_result) {
  ASSERT_TRUE(audio_perception_result.timestamp_us);
  EXPECT_EQ(*audio_perception_result.timestamp_us, 10086);

  // Validate audio localization.
  const std::optional<media_perception::AudioLocalization>& audio_localization =
      audio_perception_result.audio_localization;
  ASSERT_TRUE(audio_localization);
  ASSERT_TRUE(audio_localization->azimuth_radians);
  EXPECT_EQ(*audio_localization->azimuth_radians, 1.5);
  ASSERT_EQ(2u, audio_localization->azimuth_scores->size());
  EXPECT_EQ(audio_localization->azimuth_scores->at(0), 2.0);
  EXPECT_EQ(audio_localization->azimuth_scores->at(1), 5.0);

  // Validate audio human presence detection.
  const std::optional<media_perception::AudioHumanPresenceDetection>&
      presence_detection =
          audio_perception_result.audio_human_presence_detection;
  ASSERT_TRUE(presence_detection);
  ASSERT_TRUE(presence_detection->human_presence_likelihood);
  EXPECT_EQ(*presence_detection->human_presence_likelihood, 0.4);

  const std::optional<media_perception::AudioSpectrogram>& noise_spectrogram =
      presence_detection->noise_spectrogram;
  ASSERT_TRUE(noise_spectrogram);
  ASSERT_EQ(2u, noise_spectrogram->values->size());
  EXPECT_EQ(noise_spectrogram->values->at(0), 0.1);
  EXPECT_EQ(noise_spectrogram->values->at(1), 0.2);

  const std::optional<media_perception::AudioSpectrogram>& frame_spectrogram =
      presence_detection->frame_spectrogram;
  ASSERT_TRUE(frame_spectrogram);
  ASSERT_EQ(1u, frame_spectrogram->values->size());
  EXPECT_EQ(frame_spectrogram->values->at(0), 0.3);

  // Validate hotword detection.
  const std::optional<media_perception::HotwordDetection>& hotword_detection =
      audio_perception_result.hotword_detection;
  ASSERT_TRUE(hotword_detection);
  ASSERT_EQ(2u, hotword_detection->hotwords->size());

  const media_perception::Hotword& hotword_one =
      hotword_detection->hotwords->at(0);
  EXPECT_EQ(hotword_one.type, media_perception::HotwordType::kOkGoogle);
  ASSERT_TRUE(hotword_one.frame_id);
  EXPECT_EQ(*hotword_one.frame_id, 987);
  ASSERT_TRUE(hotword_one.start_timestamp_ms);
  EXPECT_EQ(*hotword_one.start_timestamp_ms, 10456);
  ASSERT_TRUE(hotword_one.end_timestamp_ms);
  EXPECT_EQ(*hotword_one.end_timestamp_ms, 234567);
  ASSERT_TRUE(hotword_one.confidence);
  EXPECT_FLOAT_EQ(*hotword_one.confidence, 0.9);
  ASSERT_TRUE(hotword_one.id);
  EXPECT_EQ(*hotword_one.id, 4567);

  const media_perception::Hotword& hotword_two =
      hotword_detection->hotwords->at(1);
  EXPECT_EQ(hotword_two.type, media_perception::HotwordType::kUnknownType);
  ASSERT_TRUE(hotword_two.frame_id);
  EXPECT_EQ(*hotword_two.frame_id, 789);
  ASSERT_TRUE(hotword_two.start_timestamp_ms);
  EXPECT_EQ(*hotword_two.start_timestamp_ms, 65401);
  ASSERT_TRUE(hotword_two.end_timestamp_ms);
  EXPECT_EQ(*hotword_two.end_timestamp_ms, 765432);
  ASSERT_TRUE(hotword_two.confidence);
  EXPECT_FLOAT_EQ(*hotword_two.confidence, 0.4);
  ASSERT_TRUE(hotword_two.id);
  EXPECT_EQ(*hotword_two.id, 7654);
}

void ValidateAudioVisualPerceptionResult(
    const media_perception::AudioVisualPerception& perception_result) {
  ASSERT_TRUE(perception_result.timestamp_us);
  EXPECT_EQ(*perception_result.timestamp_us, 91008);

  // Validate audio-visual human presence detection.
  const std::optional<media_perception::AudioVisualHumanPresenceDetection>&
      presence_detection =
          perception_result.audio_visual_human_presence_detection;
  ASSERT_TRUE(presence_detection);
  ASSERT_TRUE(presence_detection->human_presence_likelihood);
  EXPECT_EQ(*presence_detection->human_presence_likelihood, 0.5);
}

void InitializeFakeImageFrameData(mri::ImageFrame* image_frame) {
  image_frame->set_width(1);
  image_frame->set_height(2);
  image_frame->set_data_length(3);
  image_frame->set_pixel_data("    ");
  image_frame->set_format(mri::ImageFrame::JPEG);
}

void ValidateFakeImageFrameData(
    const media_perception::ImageFrame& image_frame_result) {
  ASSERT_TRUE(image_frame_result.width);
  EXPECT_EQ(*image_frame_result.width, 1);
  ASSERT_TRUE(image_frame_result.height);
  EXPECT_EQ(*image_frame_result.height, 2);
  ASSERT_TRUE(image_frame_result.data_length);
  EXPECT_EQ(*image_frame_result.data_length, 3);
  ASSERT_TRUE(image_frame_result.frame);
  EXPECT_EQ((*image_frame_result.frame).size(), 4ul);
  EXPECT_EQ(image_frame_result.format, media_perception::ImageFormat::kJpeg);
}

void ValidatePointIdl(const std::optional<media_perception::Point>& point,
                      float x,
                      float y) {
  ASSERT_TRUE(point);
  ASSERT_TRUE(point->x);
  EXPECT_EQ(*point->x, x);
  ASSERT_TRUE(point->y);
  EXPECT_EQ(*point->y, y);
}

void ValidatePointProto(const mri::Point& point, float x, float y) {
  ASSERT_TRUE(point.has_x());
  EXPECT_EQ(point.x(), x);
  ASSERT_TRUE(point.has_y());
  EXPECT_EQ(point.y(), y);
}

}  // namespace

// Verifies that the data is converted successfully and as expected in each of
// these cases.

TEST(MediaPerceptionConversionUtilsTest, MediaPerceptionProtoToIdl) {
  const int kFrameId = 2;
  mri::MediaPerception media_perception;
  // Fill in fake values for the media_perception proto.
  media_perception.set_timestamp(1);
  mri::FramePerception* frame_perception =
      media_perception.add_frame_perception();
  InitializeFakeFramePerception(kFrameId, frame_perception);
  mri::AudioPerception* audio_perception =
      media_perception.add_audio_perception();
  InitializeFakeAudioPerception(audio_perception);
  mri::AudioVisualPerception* audio_visual_perception =
      media_perception.add_audio_visual_perception();
  InitializeFakeAudioVisualPerception(audio_visual_perception);
  mri::Metadata* metadata = media_perception.mutable_metadata();
  InitializeFakeMetadata(metadata);
  media_perception::MediaPerception media_perception_result =
      media_perception::MediaPerceptionProtoToIdl(media_perception);
  EXPECT_EQ(*media_perception_result.timestamp, 1);
  ASSERT_TRUE(media_perception_result.frame_perceptions);
  ASSERT_EQ(1u, media_perception_result.frame_perceptions->size());
  ASSERT_TRUE(media_perception_result.audio_perceptions);
  ASSERT_EQ(1u, media_perception_result.audio_perceptions->size());
  ASSERT_TRUE(media_perception_result.metadata);
  ValidateFramePerceptionResult(
      kFrameId, media_perception_result.frame_perceptions->at(0));
  ValidateAudioPerceptionResult(
      media_perception_result.audio_perceptions->at(0));
  ValidateAudioVisualPerceptionResult(
      media_perception_result.audio_visual_perceptions->at(0));
  ValidateMetadataResult(*media_perception_result.metadata);
}

TEST(MediaPerceptionConversionUtilsTest, DiagnosticsProtoToIdl) {
  const size_t kNumSamples = 3;
  mri::Diagnostics diagnostics;
  for (size_t i = 0; i < kNumSamples; i++) {
    mri::PerceptionSample* perception_sample =
        diagnostics.add_perception_sample();
    mri::FramePerception* frame_perception =
        perception_sample->mutable_frame_perception();
    InitializeFakeFramePerception(i, frame_perception);
    mri::ImageFrame* image_frame = perception_sample->mutable_image_frame();
    InitializeFakeImageFrameData(image_frame);
    mri::Metadata* metadata = perception_sample->mutable_metadata();
    InitializeFakeMetadata(metadata);
  }
  media_perception::Diagnostics diagnostics_result =
      media_perception::DiagnosticsProtoToIdl(diagnostics);
  ASSERT_EQ(kNumSamples, diagnostics_result.perception_samples->size());
  for (size_t i = 0; i < kNumSamples; i++) {
    SCOPED_TRACE(testing::Message() << "Sample number: " << i);
    const media_perception::PerceptionSample& perception_sample_result =
        diagnostics_result.perception_samples->at(i);

    const std::optional<media_perception::FramePerception>&
        frame_perception_result = perception_sample_result.frame_perception;
    ASSERT_TRUE(frame_perception_result);

    const std::optional<media_perception::ImageFrame>& image_frame_result =
        perception_sample_result.image_frame;
    ASSERT_TRUE(image_frame_result);

    const std::optional<media_perception::Metadata>& metadata_result =
        perception_sample_result.metadata;
    ASSERT_TRUE(metadata_result);

    ValidateFramePerceptionResult(i, *frame_perception_result);
    ValidateFakeImageFrameData(*image_frame_result);
    ValidateMetadataResult(*metadata_result);
  }
}

TEST(MediaPerceptionConversionUtilsTest, StateProtoToIdl) {
  mri::State state;
  state.set_status(mri::State::RUNNING);
  state.set_configuration(kTestConfiguration);

  state.mutable_whiteboard()->mutable_top_left()->set_x(kWhiteboardTopLeftX);
  state.mutable_whiteboard()->mutable_top_left()->set_y(kWhiteboardTopLeftY);
  state.mutable_whiteboard()->mutable_top_right()->set_x(kWhiteboardTopRightX);
  state.mutable_whiteboard()->mutable_top_right()->set_y(kWhiteboardTopRightY);
  state.mutable_whiteboard()->mutable_bottom_left()->set_x(
      kWhiteboardBottomLeftX);
  state.mutable_whiteboard()->mutable_bottom_left()->set_y(
      kWhiteboardBottomLeftY);
  state.mutable_whiteboard()->mutable_bottom_right()->set_x(
      kWhiteboardBottomRightX);
  state.mutable_whiteboard()->mutable_bottom_right()->set_y(
      kWhiteboardBottomRightY);
  state.mutable_whiteboard()->set_aspect_ratio(kWhiteboardAspectRatio);

  state.add_features(mri::State::FEATURE_AUTOZOOM);
  state.add_features(mri::State::FEATURE_HOTWORD_DETECTION);
  state.add_features(mri::State::FEATURE_OCCUPANCY_DETECTION);
  state.add_features(mri::State::FEATURE_EDGE_EMBEDDINGS);
  state.add_features(mri::State::FEATURE_SOFTWARE_CROPPING);
  state.add_features(mri::State::FEATURE_UNSET);

  // Number NamedTemplateArgument.
  mri::State::NamedTemplateArgument* named_template_argument_proto =
      state.add_named_template_arguments();
  named_template_argument_proto->set_name(kNumericalTemplateArgumentName);
  named_template_argument_proto->set_num(kNumericalTemplateArgumentValue);

  // Empty
  named_template_argument_proto = state.add_named_template_arguments();

  // String NamedTemplateArgument.
  named_template_argument_proto = state.add_named_template_arguments();
  named_template_argument_proto->set_name(kStringTemplateArgumentName);
  named_template_argument_proto->set_str(kStringTemplateArgumentValue);

  media_perception::State state_result =
      media_perception::StateProtoToIdl(state);
  EXPECT_EQ(state_result.status, media_perception::Status::kRunning);
  ASSERT_TRUE(state_result.configuration);
  EXPECT_EQ(*state_result.configuration, kTestConfiguration);

  ASSERT_TRUE(state_result.whiteboard);
  ValidatePointIdl(state_result.whiteboard->top_left, kWhiteboardTopLeftX,
                   kWhiteboardTopLeftY);
  ValidatePointIdl(state_result.whiteboard->top_right, kWhiteboardTopRightX,
                   kWhiteboardTopRightY);
  ValidatePointIdl(state_result.whiteboard->bottom_left, kWhiteboardBottomLeftX,
                   kWhiteboardBottomLeftY);
  ValidatePointIdl(state_result.whiteboard->bottom_right,
                   kWhiteboardBottomRightX, kWhiteboardBottomRightY);
  ASSERT_TRUE(state_result.whiteboard->aspect_ratio);
  EXPECT_EQ(*state_result.whiteboard->aspect_ratio, kWhiteboardAspectRatio);

  ASSERT_TRUE(state_result.features);
  ASSERT_TRUE(state_result.features);
  ASSERT_EQ(state_result.features->size(), 5u);
  EXPECT_EQ(state_result.features->at(0), media_perception::Feature::kAutozoom);
  EXPECT_EQ(state_result.features->at(1),
            media_perception::Feature::kHotwordDetection);
  EXPECT_EQ(state_result.features->at(2),
            media_perception::Feature::kOccupancyDetection);
  EXPECT_EQ(state_result.features->at(3),
            media_perception::Feature::kEdgeEmbeddings);
  EXPECT_EQ(state_result.features->at(4),
            media_perception::Feature::kSoftwareCropping);

  ASSERT_EQ(state_result.named_template_arguments->size(),
            kNamedTemplateArgumentsSize);

  // Number.

  EXPECT_EQ(*state_result.named_template_arguments->at(0).name,
            kNumericalTemplateArgumentName);
  EXPECT_NEAR(*state_result.named_template_arguments->at(0).value->as_number,
              kNumericalTemplateArgumentValue, kDoubleTolerance);
  EXPECT_FALSE(state_result.named_template_arguments->at(0).value->as_string);

  // Empty.
  EXPECT_EQ(*state_result.named_template_arguments->at(1).name, "");
  EXPECT_FALSE(state_result.named_template_arguments->at(1).value);

  // String.
  EXPECT_EQ(*state_result.named_template_arguments->at(2).name,
            kStringTemplateArgumentName);
  EXPECT_FALSE(state_result.named_template_arguments->at(2)
                   .value->as_number.has_value());
  EXPECT_EQ(*state_result.named_template_arguments->at(2).value->as_string,
            kStringTemplateArgumentValue);

  state.set_status(mri::State::STARTED);
  state.set_device_context(kTestDeviceContext);
  state_result = media_perception::StateProtoToIdl(state);
  EXPECT_EQ(state_result.status, media_perception::Status::kStarted);
  ASSERT_TRUE(state_result.device_context);
  EXPECT_EQ(*state_result.device_context, kTestDeviceContext);

  state.set_status(mri::State::RESTARTING);
  state_result = media_perception::StateProtoToIdl(state);
  EXPECT_EQ(state_result.status, media_perception::Status::kRestarting);

  state.set_status(mri::State::STOPPED);
  state_result = media_perception::StateProtoToIdl(state);
  EXPECT_EQ(state_result.status, media_perception::Status::kStopped);
}

TEST(MediaPerceptionConversionUtilsTest, StateIdlToProto) {
  media_perception::State state;
  state.status = media_perception::Status::kUninitialized;
  mri::State state_proto = StateIdlToProto(state);
  EXPECT_EQ(state_proto.status(), mri::State::UNINITIALIZED);
  EXPECT_FALSE(state_proto.has_device_context());

  state.status = media_perception::Status::kRunning;
  state.configuration = kTestConfiguration;
  state.whiteboard.emplace();
  state.whiteboard->top_left =
      MakePointIdl(kWhiteboardTopLeftX, kWhiteboardTopLeftY);
  state.whiteboard->top_right =
      MakePointIdl(kWhiteboardTopRightX, kWhiteboardTopRightY);
  state.whiteboard->bottom_left =
      MakePointIdl(kWhiteboardBottomLeftX, kWhiteboardBottomLeftY);
  state.whiteboard->bottom_right =
      MakePointIdl(kWhiteboardBottomRightX, kWhiteboardBottomRightY);
  state.whiteboard->aspect_ratio = kWhiteboardAspectRatio;

  state.features.emplace();
  state.features->emplace_back(media_perception::Feature::kAutozoom);
  state.features->emplace_back(media_perception::Feature::kHotwordDetection);
  state.features->emplace_back(media_perception::Feature::kOccupancyDetection);
  state.features->emplace_back(media_perception::Feature::kEdgeEmbeddings);
  state.features->emplace_back(media_perception::Feature::kSoftwareCropping);
  state.features->emplace_back(media_perception::Feature::kNone);

  // {Number, Empty, String} test cases.
  state.named_template_arguments =
      std::vector<media_perception::NamedTemplateArgument>(
          kNamedTemplateArgumentsSize);

  state.named_template_arguments->at(0).name = kNumericalTemplateArgumentName;

  state.named_template_arguments->at(0).value =
      media_perception::NamedTemplateArgument::Value::FromValue(
          base::Value(kNumericalTemplateArgumentValue));

  state.named_template_arguments->at(2).name = kStringTemplateArgumentName;
  state.named_template_arguments->at(2).value =
      media_perception::NamedTemplateArgument::Value::FromValue(
          base::Value(kStringTemplateArgumentValue));

  state_proto = StateIdlToProto(state);
  EXPECT_EQ(state_proto.status(), mri::State::RUNNING);
  ASSERT_TRUE(state_proto.has_configuration());
  EXPECT_EQ(state_proto.configuration(), kTestConfiguration);
  ASSERT_TRUE(state_proto.has_whiteboard());
  ASSERT_TRUE(state_proto.whiteboard().has_top_left());
  ValidatePointProto(state_proto.whiteboard().top_left(), kWhiteboardTopLeftX,
                     kWhiteboardTopLeftY);
  ASSERT_TRUE(state_proto.whiteboard().has_top_right());
  ValidatePointProto(state_proto.whiteboard().top_right(), kWhiteboardTopRightX,
                     kWhiteboardTopRightY);
  ASSERT_TRUE(state_proto.whiteboard().has_bottom_left());
  ValidatePointProto(state_proto.whiteboard().bottom_left(),
                     kWhiteboardBottomLeftX, kWhiteboardBottomLeftY);
  ASSERT_TRUE(state_proto.whiteboard().has_bottom_right());
  ValidatePointProto(state_proto.whiteboard().bottom_right(),
                     kWhiteboardBottomRightX, kWhiteboardBottomRightY);
  ASSERT_TRUE(state_proto.whiteboard().has_aspect_ratio());
  EXPECT_EQ(state_proto.whiteboard().aspect_ratio(), kWhiteboardAspectRatio);

  ASSERT_EQ(state_proto.features_size(), 6);
  EXPECT_EQ(state_proto.features(0), mri::State::FEATURE_AUTOZOOM);
  EXPECT_EQ(state_proto.features(1), mri::State::FEATURE_HOTWORD_DETECTION);
  EXPECT_EQ(state_proto.features(2), mri::State::FEATURE_OCCUPANCY_DETECTION);
  EXPECT_EQ(state_proto.features(3), mri::State::FEATURE_EDGE_EMBEDDINGS);
  EXPECT_EQ(state_proto.features(4), mri::State::FEATURE_SOFTWARE_CROPPING);
  EXPECT_EQ(state_proto.features(5), mri::State::FEATURE_UNSET);

  ASSERT_EQ(state_proto.named_template_arguments_size(),
            static_cast<int>(kNamedTemplateArgumentsSize));

  EXPECT_EQ(state_proto.named_template_arguments(0).name(),
            kNumericalTemplateArgumentName);
  EXPECT_NEAR(state_proto.named_template_arguments(0).num(),
              kNumericalTemplateArgumentValue, kDoubleTolerance);

  EXPECT_FALSE(state_proto.named_template_arguments(1).has_name());
  EXPECT_FALSE(state_proto.named_template_arguments(1).has_str());
  EXPECT_FALSE(state_proto.named_template_arguments(1).has_num());

  EXPECT_EQ(state_proto.named_template_arguments(2).name(),
            kStringTemplateArgumentName);
  EXPECT_EQ(state_proto.named_template_arguments(2).str(),
            kStringTemplateArgumentValue);

  state.status = media_perception::Status::kSuspended;
  state.device_context = kTestDeviceContext;
  state_proto = StateIdlToProto(state);
  EXPECT_EQ(state_proto.status(), mri::State::SUSPENDED);
  EXPECT_EQ(state_proto.device_context(), kTestDeviceContext);

  state.status = media_perception::Status::kRestarting;
  state_proto = StateIdlToProto(state);
  EXPECT_EQ(state_proto.status(), mri::State::RESTARTING);

  state.status = media_perception::Status::kStopped;
  state_proto = StateIdlToProto(state);
  EXPECT_EQ(mri::State::STOPPED, state_proto.status());
}

TEST(MediaPerceptionConversionUtilsTest, StateIdlToProtoWithVideoStreamParam) {
  media_perception::State state;
  state.status = media_perception::Status::kRunning;
  state.video_stream_param = std::vector<media_perception::VideoStreamParam>(2);
  InitializeVideoStreamParam(
      state.video_stream_param->at(0), kVideoStreamIdForFaceDetection,
      kVideoStreamWidthForFaceDetection, kVideoStreamHeightForFaceDetection,
      kVideoStreamFrameRateForFaceDetection);

  InitializeVideoStreamParam(
      state.video_stream_param->at(1), kVideoStreamIdForVideoCapture,
      kVideoStreamWidthForVideoCapture, kVideoStreamHeightForVideoCapture,
      kVideoStreamFrameRateForVideoCapture);

  mri::State state_proto = StateIdlToProto(state);
  EXPECT_EQ(state_proto.status(), mri::State::RUNNING);

  EXPECT_EQ(kVideoStreamIdForFaceDetection,
            state_proto.video_stream_param(0).id());
  EXPECT_EQ(kVideoStreamWidthForFaceDetection,
            state_proto.video_stream_param(0).width());
  EXPECT_EQ(kVideoStreamHeightForFaceDetection,
            state_proto.video_stream_param(0).height());
  EXPECT_EQ(kVideoStreamFrameRateForFaceDetection,
            state_proto.video_stream_param(0).frame_rate());

  EXPECT_EQ(kVideoStreamIdForVideoCapture,
            state_proto.video_stream_param(1).id());
  EXPECT_EQ(kVideoStreamWidthForVideoCapture,
            state_proto.video_stream_param(1).width());
  EXPECT_EQ(kVideoStreamHeightForVideoCapture,
            state_proto.video_stream_param(1).height());
  EXPECT_EQ(kVideoStreamFrameRateForVideoCapture,
            state_proto.video_stream_param(1).frame_rate());
}

}  // namespace extensions
