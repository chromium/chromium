// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_annotation_agent.h"

#include "pdf/test/fake_annotation_agent_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {
namespace {

using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;

class MockPDFAnnogationAgentContainer : public PdfAnnotationAgent::Container {
 public:
  MOCK_METHOD(bool,
              FindAndHighlightTextFragments,
              (base::span<const std::string> text_fragments),
              (override));
  MOCK_METHOD(void, ScrollTextFragmentIntoView, (), (override));
  MOCK_METHOD(void, RemoveTextFragments, (), (override));
};

class PdfAnnotationAgentTest : public ::testing::Test {
 public:
  ~PdfAnnotationAgentTest() override = default;

  void CreateAgent(blink::mojom::SelectorPtr selector) {
    fake_annotation_agent_host_.reset();
    // IPC disconnection is asynchronous. FlushForTesting() does not work.
    base::RunLoop().RunUntilIdle();

    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost>
        annotation_agent_host_receiver;
    mojo::PendingRemote<blink::mojom::AnnotationAgent> annotation_agent_remote;
    pdf_annotation_agent_ = std::make_unique<PdfAnnotationAgent>(
        &mock_container_, blink::mojom::AnnotationType::kGlic,
        std::move(selector),
        annotation_agent_host_receiver.InitWithNewPipeAndPassRemote(),
        annotation_agent_remote.InitWithNewPipeAndPassReceiver());
    fake_annotation_agent_host_ = std::make_unique<FakeAnnotationAgentHost>(
        std::move(annotation_agent_host_receiver),
        std::move(annotation_agent_remote));
  }

 protected:
  NiceMock<MockPDFAnnogationAgentContainer> mock_container_;
  std::unique_ptr<FakeAnnotationAgentHost> fake_annotation_agent_host_;

 private:
  std::unique_ptr<PdfAnnotationAgent> pdf_annotation_agent_;
};

}  // namespace

TEST_F(PdfAnnotationAgentTest, TextFragmentFound) {
  EXPECT_CALL(mock_container_,
              FindAndHighlightTextFragments(ElementsAre("does_not_matter")))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_container_, ScrollTextFragmentIntoView).Times(0);
  CreateAgent(blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
  EXPECT_EQ(fake_annotation_agent_host_->WaitForAttachmentResult(),
            blink::mojom::AttachmentResult::kSuccess);
}

TEST_F(PdfAnnotationAgentTest, TextFragmentNotFound) {
  EXPECT_CALL(mock_container_,
              FindAndHighlightTextFragments(ElementsAre("does_not_matter")))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_container_, ScrollTextFragmentIntoView).Times(0);
  CreateAgent(blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
  EXPECT_EQ(fake_annotation_agent_host_->WaitForAttachmentResult(),
            blink::mojom::AttachmentResult::kSelectorNotMatched);
}

TEST_F(PdfAnnotationAgentTest, EmptySelector) {
  EXPECT_CALL(mock_container_, FindAndHighlightTextFragments).Times(0);
  EXPECT_CALL(mock_container_, ScrollTextFragmentIntoView).Times(0);
  CreateAgent(blink::mojom::Selector::NewSerializedSelector(""));
}

TEST_F(PdfAnnotationAgentTest, NodeSelector) {
  EXPECT_CALL(mock_container_, FindAndHighlightTextFragments).Times(0);
  EXPECT_CALL(mock_container_, ScrollTextFragmentIntoView).Times(0);
  CreateAgent(blink::mojom::Selector::NewNodeId(1));
}

TEST_F(PdfAnnotationAgentTest, ScrollIntoView) {
  EXPECT_CALL(mock_container_, FindAndHighlightTextFragments)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_container_, ScrollTextFragmentIntoView);
  CreateAgent(blink::mojom::Selector::NewSerializedSelector("does_not_matter"));
  fake_annotation_agent_host_->ScrollIntoView();
}

}  // namespace chrome_pdf
