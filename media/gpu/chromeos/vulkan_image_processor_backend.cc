// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/vulkan_image_processor_backend.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "media/base/scopedfd_helper.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/chromeos/shaders/shaders.h"
#include "media/gpu/macros.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

class VulkanImageProcessorBackend::VulkanRenderPass {
 public:
  VulkanRenderPass(const VulkanRenderPass&) = delete;
  VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

  ~VulkanRenderPass();

  static std::unique_ptr<VulkanRenderPass> Create(VkFormat format,
                                                  VkDevice logical_device);

  VkRenderPass Get();

 private:
  VulkanRenderPass(VkDevice logical_device, VkRenderPass render_pass);

  const VkRenderPass render_pass_;

  const VkDevice logical_device_;
};

class VulkanImageProcessorBackend::VulkanShader {
 public:
  VulkanShader(const VulkanShader&) = delete;
  VulkanShader& operator=(const VulkanShader&) = delete;

  ~VulkanShader();

  static std::unique_ptr<VulkanShader> Create(const uint32_t* shader_code,
                                              size_t shader_code_size,
                                              VkDevice logical_device);

  VkShaderModule Get();

 private:
  VulkanShader(VkDevice logical_device, VkShaderModule shader);

  const VkShaderModule shader_;

  const VkDevice logical_device_;
};

class VulkanImageProcessorBackend::VulkanPipeline {
 public:
  VulkanPipeline(const VulkanPipeline&) = delete;
  VulkanPipeline& operator=(const VulkanPipeline&) = delete;

  ~VulkanPipeline();

  static std::unique_ptr<VulkanPipeline> Create(
      const std::vector<VkVertexInputBindingDescription>& binding_descriptions,
      const std::vector<VkVertexInputAttributeDescription>&
          attribute_descriptions,
      const std::vector<VkDescriptorSetLayoutBinding>& ubo_bindings,
      std::unique_ptr<VulkanShader> vert_shader,
      std::unique_ptr<VulkanShader> frag_shader,
      const gfx::Size& output_size,
      size_t push_constants_size,
      VkRenderPass render_pass,
      VkDevice logical_device);

  VkPipeline Get();
  VkDescriptorSetLayout GetDescriptorSetLayout();
  VkPipelineLayout GetPipelineLayout();

 private:
  VulkanPipeline(VkPipeline pipeline,
                 VkDescriptorSetLayout descriptor_set_layout,
                 VkPipelineLayout pipeline_layout,
                 std::unique_ptr<VulkanShader> vert_shader,
                 std::unique_ptr<VulkanShader> frag_shader,
                 VkDevice logical_device);

  const VkPipeline pipeline_;
  const VkDescriptorSetLayout descriptor_set_layout_;
  const VkPipelineLayout pipeline_layout_;

  const std::unique_ptr<VulkanShader> vert_shader_;
  const std::unique_ptr<VulkanShader> frag_shader_;
  const VkDevice logical_device_;
};

class VulkanImageProcessorBackend::VulkanDescriptorPool {
 public:
  VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
  VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;

  ~VulkanDescriptorPool();

  static std::unique_ptr<VulkanDescriptorPool> Create(
      size_t num_descriptor_sets,
      std::vector<VkDescriptorType> descriptor_types,
      VkDescriptorSetLayout descriptor_set_layout,
      VkDevice logical_device);

  const std::vector<VkDescriptorSet>& Get();

 private:
  VulkanDescriptorPool(std::vector<VkDescriptorSet> descriptor_sets,
                       VkDescriptorPool descriptor_pool,
                       VkDevice logical_device);

  const std::vector<VkDescriptorSet> descriptor_sets_;

  const VkDescriptorPool descriptor_pool_;
  const VkDevice logical_device_;
};

class VulkanImageProcessorBackend::VulkanDeviceQueueWrapper {
 public:
  VulkanDeviceQueueWrapper(const VulkanDeviceQueueWrapper&) = delete;
  VulkanDeviceQueueWrapper& operator=(const VulkanDeviceQueueWrapper&) = delete;

  ~VulkanDeviceQueueWrapper();

  static std::unique_ptr<VulkanDeviceQueueWrapper> Create(
      gpu::VulkanImplementation* implementation);

  gpu::VulkanDeviceQueue* GetVulkanDeviceQueue();
  VkPhysicalDevice GetVulkanPhysicalDevice();
  VkPhysicalDeviceProperties GetVulkanPhysicalDeviceProperties();
  VkDevice GetVulkanDevice();
  VkQueue GetVulkanQueue();
  uint32_t GetVulkanQueueIndex();

 private:
  explicit VulkanDeviceQueueWrapper(
      std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue);

  const std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue_;
};

class VulkanImageProcessorBackend::VulkanCommandPoolWrapper {
 public:
  VulkanCommandPoolWrapper(const VulkanCommandPoolWrapper&) = delete;
  VulkanCommandPoolWrapper& operator=(const VulkanCommandPoolWrapper&) = delete;

  ~VulkanCommandPoolWrapper();

  static std::unique_ptr<VulkanCommandPoolWrapper> Create(
      gpu::VulkanDeviceQueue* device_queue);

  gpu::VulkanCommandBuffer* GetPrimaryCommandBuffer();

 private:
  VulkanCommandPoolWrapper(
      std::unique_ptr<gpu::VulkanCommandPool> command_pool,
      std::unique_ptr<gpu::VulkanCommandBuffer> primary_command_buf);

  const std::unique_ptr<gpu::VulkanCommandPool> command_pool_;
  std::unique_ptr<gpu::VulkanCommandBuffer> primary_command_buf_;
};

class VulkanImageProcessorBackend::VulkanTextureImage {
 public:
  VulkanTextureImage(const VulkanTextureImage&) = delete;
  VulkanTextureImage& operator=(const VulkanTextureImage&) = delete;

  ~VulkanTextureImage();

  static std::unique_ptr<VulkanTextureImage> Create(
      std::unique_ptr<gpu::VulkanImage> image,
      const std::vector<VkFormat>& formats,
      const std::vector<gfx::Size>& sizes,
      const std::vector<VkImageAspectFlagBits>& aspects,
      bool is_framebuffer,
      VkRenderPass render_pass,
      VkDevice logical_device);

  VkImage GetImage();
  const std::vector<VkImageView>& GetImageViews();
  const std::vector<VkFramebuffer>& GetFramebuffers();
  void TransitionImageLayout(
      gpu::VulkanCommandBuffer* command_buf,
      VkImageLayout new_layout,
      uint32_t src_queue_family_index = VK_QUEUE_FAMILY_IGNORED,
      uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED);

 private:
  VulkanTextureImage(std::unique_ptr<gpu::VulkanImage> image,
                     const std::vector<VkImageView>& image_views,
                     const std::vector<VkFramebuffer>& framebuffers,
                     VkImageLayout initial_layout,
                     VkDevice logical_device);

  const std::unique_ptr<gpu::VulkanImage> image_;
  const std::vector<VkImageView> image_views_;
  const std::vector<VkFramebuffer> framebuffers_;

  VkImageLayout current_layout_;
  const VkDevice logical_device_;
};

VulkanImageProcessorBackend::VulkanRenderPass::VulkanRenderPass(
    VkDevice logical_device,
    VkRenderPass render_pass)
    : render_pass_(render_pass), logical_device_(logical_device) {}

VulkanImageProcessorBackend::VulkanRenderPass::~VulkanRenderPass() {
  vkDestroyRenderPass(logical_device_, render_pass_, nullptr);
}

VkRenderPass VulkanImageProcessorBackend::VulkanRenderPass::Get() {
  return render_pass_;
}

std::unique_ptr<VulkanImageProcessorBackend::VulkanRenderPass>
VulkanImageProcessorBackend::VulkanRenderPass::Create(VkFormat format,
                                                      VkDevice logical_device) {
  VkAttachmentDescription color_attachment{};
  color_attachment.format = format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;

  VkRenderPass render_pass;
  if (vkCreateRenderPass(logical_device, &render_pass_info, nullptr,
                         &render_pass) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create render pass!";
    return nullptr;
  }

  return base::WrapUnique(new VulkanRenderPass(logical_device, render_pass));
}

VulkanImageProcessorBackend::VulkanShader::VulkanShader(VkDevice logical_device,
                                                        VkShaderModule shader)
    : shader_(shader), logical_device_(logical_device) {}

VulkanImageProcessorBackend::VulkanShader::~VulkanShader() {
  vkDestroyShaderModule(logical_device_, shader_, nullptr);
}

VkShaderModule VulkanImageProcessorBackend::VulkanShader::Get() {
  return shader_;
}

std::unique_ptr<VulkanImageProcessorBackend::VulkanShader>
VulkanImageProcessorBackend::VulkanShader::Create(const uint32_t* shader_code,
                                                  size_t shader_code_size,
                                                  VkDevice logical_device) {
  VkShaderModuleCreateInfo shader_info{};
  shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_info.codeSize = shader_code_size;
  shader_info.pCode = shader_code;

  VkShaderModule shader;
  if (vkCreateShaderModule(logical_device, &shader_info, nullptr, &shader) !=
      VK_SUCCESS) {
    LOG(ERROR) << "Could not create shader module!";
    return nullptr;
  }

  return base::WrapUnique(new VulkanShader(logical_device, shader));
}

VulkanImageProcessorBackend::VulkanPipeline::VulkanPipeline(
    VkPipeline pipeline,
    VkDescriptorSetLayout descriptor_set_layout,
    VkPipelineLayout pipeline_layout,
    std::unique_ptr<VulkanImageProcessorBackend::VulkanShader> vert_shader,
    std::unique_ptr<VulkanImageProcessorBackend::VulkanShader> frag_shader,
    VkDevice logical_device)
    : pipeline_(pipeline),
      descriptor_set_layout_(descriptor_set_layout),
      pipeline_layout_(pipeline_layout),
      vert_shader_(std::move(vert_shader)),
      frag_shader_(std::move(frag_shader)),
      logical_device_(logical_device) {}

VulkanImageProcessorBackend::VulkanPipeline::~VulkanPipeline() {
  vkDestroyPipeline(logical_device_, pipeline_, nullptr);
  vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);
  vkDestroyDescriptorSetLayout(logical_device_, descriptor_set_layout_,
                               nullptr);
}

VkPipeline VulkanImageProcessorBackend::VulkanPipeline::Get() {
  return pipeline_;
}

VkDescriptorSetLayout
VulkanImageProcessorBackend::VulkanPipeline::GetDescriptorSetLayout() {
  return descriptor_set_layout_;
}

VkPipelineLayout
VulkanImageProcessorBackend::VulkanPipeline::GetPipelineLayout() {
  return pipeline_layout_;
}

std::unique_ptr<VulkanImageProcessorBackend::VulkanPipeline>
VulkanImageProcessorBackend::VulkanPipeline::Create(
    const std::vector<VkVertexInputBindingDescription>& binding_descriptions,
    const std::vector<VkVertexInputAttributeDescription>&
        attribute_descriptions,
    const std::vector<VkDescriptorSetLayoutBinding>& ubo_bindings,
    std::unique_ptr<VulkanImageProcessorBackend::VulkanShader> vert_shader,
    std::unique_ptr<VulkanImageProcessorBackend::VulkanShader> frag_shader,
    const gfx::Size& output_size,
    size_t push_constants_size,
    VkRenderPass render_pass,
    VkDevice logical_device) {
  VkDescriptorSetLayoutCreateInfo descriptor_layout_info{};
  descriptor_layout_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_layout_info.bindingCount = ubo_bindings.size();
  descriptor_layout_info.pBindings = ubo_bindings.data();

  VkDescriptorSetLayout descriptor_set_layout;
  if (vkCreateDescriptorSetLayout(logical_device, &descriptor_layout_info,
                                  nullptr,
                                  &descriptor_set_layout) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create descriptor set layout!";
    return nullptr;
  }

  VkPushConstantRange push_constant_range;
  push_constant_range.offset = 0;
  push_constant_range.size = push_constants_size;
  push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
  if (push_constants_size) {
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;
    pipeline_layout_info.pushConstantRangeCount = 1;
  }

  VkPipelineLayout pipeline_layout;
  if (vkCreatePipelineLayout(logical_device, &pipeline_layout_info, nullptr,
                             &pipeline_layout) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create pipeline layout!";
    return nullptr;
  }

  VkPipelineShaderStageCreateInfo shader_stages[2] = {};

  VkPipelineShaderStageCreateInfo& vert_shader_info = shader_stages[0];
  vert_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_shader_info.module = vert_shader->Get();
  vert_shader_info.pName = "main";

  VkPipelineShaderStageCreateInfo& frag_shader_info = shader_stages[1];
  frag_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_shader_info.module = frag_shader->Get();
  frag_shader_info.pName = "main";

  VkPipelineVertexInputStateCreateInfo vertex_input_info{};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_info.vertexBindingDescriptionCount = binding_descriptions.size();
  vertex_input_info.pVertexBindingDescriptions = binding_descriptions.data();
  vertex_input_info.vertexAttributeDescriptionCount =
      attribute_descriptions.size();
  vertex_input_info.pVertexAttributeDescriptions =
      attribute_descriptions.data();

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)output_size.width();
  viewport.height = (float)output_size.height();
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {static_cast<uint32_t>(output_size.width()),
                    static_cast<uint32_t>(output_size.height())};

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount =
      static_cast<uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pDepthStencilState = nullptr;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = render_pass;
  pipeline_info.subpass = 0;

  VkPipeline pipeline;
  if (vkCreateGraphicsPipelines(logical_device, VK_NULL_HANDLE, 1,
                                &pipeline_info, nullptr,
                                &pipeline) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create graphics pipeline!";
    return nullptr;
  }

  return base::WrapUnique(new VulkanPipeline(
      pipeline, descriptor_set_layout, pipeline_layout, std::move(vert_shader),
      std::move(frag_shader), logical_device));
}

VulkanImageProcessorBackend::VulkanDescriptorPool::VulkanDescriptorPool(
    std::vector<VkDescriptorSet> descriptor_sets,
    VkDescriptorPool descriptor_pool,
    VkDevice logical_device)
    : descriptor_sets_(descriptor_sets),
      descriptor_pool_(descriptor_pool),
      logical_device_(logical_device) {}

VulkanImageProcessorBackend::VulkanDescriptorPool::~VulkanDescriptorPool() {
  vkDestroyDescriptorPool(logical_device_, descriptor_pool_, nullptr);
}

const std::vector<VkDescriptorSet>&
VulkanImageProcessorBackend::VulkanDescriptorPool::Get() {
  return descriptor_sets_;
}

std::unique_ptr<VulkanImageProcessorBackend::VulkanDescriptorPool>
VulkanImageProcessorBackend::VulkanDescriptorPool::Create(
    size_t num_descriptor_sets,
    std::vector<VkDescriptorType> descriptor_types,
    VkDescriptorSetLayout descriptor_set_layout,
    VkDevice logical_device) {
  std::vector<VkDescriptorPoolSize> pool_sizes;
  for (auto& type : descriptor_types) {
    VkDescriptorPoolSize pool_size;
    pool_size.type = type;
    pool_size.descriptorCount = num_descriptor_sets;
    pool_sizes.push_back(pool_size);
  }

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();
  pool_info.maxSets = num_descriptor_sets;

  VkDescriptorPool descriptor_pool;
  if (vkCreateDescriptorPool(logical_device, &pool_info, nullptr,
                             &descriptor_pool) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create descriptor pool!";
    return nullptr;
  }

  std::vector<VkDescriptorSetLayout> layouts(num_descriptor_sets,
                                             descriptor_set_layout);
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool;
  alloc_info.descriptorSetCount = num_descriptor_sets;
  alloc_info.pSetLayouts = layouts.data();

  std::vector<VkDescriptorSet> descriptor_sets(num_descriptor_sets);
  if (vkAllocateDescriptorSets(logical_device, &alloc_info,
                               descriptor_sets.data()) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create descriptor sets!";
    return nullptr;
  }

  return base::WrapUnique(new VulkanDescriptorPool(
      descriptor_sets, descriptor_pool, logical_device));
}

VulkanImageProcessorBackend::VulkanTextureImage::VulkanTextureImage(
    std::unique_ptr<gpu::VulkanImage> image,
    const std::vector<VkImageView>& image_views,
    const std::vector<VkFramebuffer>& framebuffers,
    VkImageLayout initial_layout,
    VkDevice logical_device)
    : image_(std::move(image)),
      image_views_(image_views),
      framebuffers_(framebuffers),
      current_layout_(initial_layout),
      logical_device_(logical_device) {}

VulkanImageProcessorBackend::VulkanTextureImage::~VulkanTextureImage() {
  for (VkFramebuffer framebuffer : framebuffers_) {
    vkDestroyFramebuffer(logical_device_, framebuffer, nullptr);
  }

  for (VkImageView image_view : image_views_) {
    vkDestroyImageView(logical_device_, image_view, nullptr);
  }

  image_->Destroy();
}

VkImage VulkanImageProcessorBackend::VulkanTextureImage::GetImage() {
  return image_->image();
}

const std::vector<VkImageView>&
VulkanImageProcessorBackend::VulkanTextureImage::GetImageViews() {
  return image_views_;
}

const std::vector<VkFramebuffer>&
VulkanImageProcessorBackend::VulkanTextureImage::GetFramebuffers() {
  return framebuffers_;
}

void VulkanImageProcessorBackend::VulkanTextureImage::TransitionImageLayout(
    gpu::VulkanCommandBuffer* command_buf,
    VkImageLayout new_layout,
    uint32_t src_queue_family_index,
    uint32_t dst_queue_family_index) {
  if (new_layout == current_layout_) {
    return;
  }

  command_buf->TransitionImageLayout(image_->image(), current_layout_,
                                     new_layout, src_queue_family_index,
                                     dst_queue_family_index);

  current_layout_ = new_layout;
}

std::unique_ptr<VulkanImageProcessorBackend::VulkanTextureImage>
VulkanImageProcessorBackend::VulkanTextureImage::Create(
    std::unique_ptr<gpu::VulkanImage> image,
    const std::vector<VkFormat>& formats,
    const std::vector<gfx::Size>& sizes,
    const std::vector<VkImageAspectFlagBits>& aspects,
    bool is_framebuffer,
    VkRenderPass render_pass,
    VkDevice logical_device) {
  CHECK_EQ(formats.size(), sizes.size());
  CHECK_EQ(sizes.size(), aspects.size());

  std::vector<VkImageView> image_views;
  for (size_t i = 0; i < formats.size(); i++) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image->image();
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = formats[i];
    view_info.subresourceRange.aspectMask = aspects[i];
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(logical_device, &view_info, nullptr, &image_view) !=
        VK_SUCCESS) {
      LOG(ERROR) << "Could not create image view!";
      return nullptr;
    }

    image_views.emplace_back(std::move(image_view));
  }

  std::vector<VkFramebuffer> framebuffers;
  if (is_framebuffer) {
    for (size_t i = 0; i < sizes.size(); i++) {
      VkFramebufferCreateInfo framebuffer_info{};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = render_pass;
      framebuffer_info.attachmentCount = 1;
      framebuffer_info.pAttachments = image_views.data() + i;
      framebuffer_info.width = sizes[i].width();
      framebuffer_info.height = sizes[i].height();
      framebuffer_info.layers = 1;

      VkFramebuffer framebuffer;
      if (vkCreateFramebuffer(logical_device, &framebuffer_info, nullptr,
                              &framebuffer) != VK_SUCCESS) {
        LOG(ERROR) << "Could not create framebuffer!";
      }

      framebuffers.emplace_back(std::move(framebuffer));
    }
  }

  return base::WrapUnique(new VulkanTextureImage(
      std::move(image), std::move(image_views), std::move(framebuffers),
      is_framebuffer ? VK_IMAGE_LAYOUT_UNDEFINED
                     : VK_IMAGE_LAYOUT_PREINITIALIZED,
      logical_device));
}

VulkanImageProcessorBackend::VulkanDeviceQueueWrapper::VulkanDeviceQueueWrapper(
    std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue)
    : vulkan_device_queue_(std::move(vulkan_device_queue)) {}

VulkanImageProcessorBackend::VulkanDeviceQueueWrapper::
    ~VulkanDeviceQueueWrapper() {
  vulkan_device_queue_->Destroy();
}

gpu::VulkanDeviceQueue*
VulkanImageProcessorBackend::VulkanDeviceQueueWrapper::GetVulkanDeviceQueue() {
  return vulkan_device_queue_.get();
}

VkPhysicalDevice VulkanImageProcessorBackend::VulkanDeviceQueueWrapper::
    GetVulkanPhysicalDevice() {
  return vulkan_device_queue_->GetVulkanPhysicalDevice();
}

VkPhysicalDeviceProperties VulkanImageProcessorBackend::
    VulkanDeviceQueueWrapper::GetVulkanPhysicalDeviceProperties() {
  return vulkan_device_queue_->vk_physical_device_properties();
}

VkDevice
VulkanImageProcessorBackend::VulkanDeviceQueueWrapper::GetVulkanDevice() {
  return vulkan_device_queue_->GetVulkanDevice();
}

VkQueue
VulkanImageProcessorBackend::VulkanDeviceQueueWrapper::GetVulkanQueue() {
  return vulkan_device_queue_->GetVulkanQueue();
}

uint32_t
VulkanImageProcessorBackend::VulkanDeviceQueueWrapper::GetVulkanQueueIndex() {
  return vulkan_device_queue_->GetVulkanQueueIndex();
}

std::unique_ptr<VulkanImageProcessorBackend::VulkanDeviceQueueWrapper>
VulkanImageProcessorBackend::VulkanDeviceQueueWrapper::Create(
    gpu::VulkanImplementation* implementation) {
  auto vulkan_device_queue = CreateVulkanDeviceQueue(
      implementation,
      gpu::VulkanDeviceQueue::DeviceQueueOption::GRAPHICS_QUEUE_FLAG);

  return base::WrapUnique(
      new VulkanDeviceQueueWrapper(std::move(vulkan_device_queue)));
}

VulkanImageProcessorBackend::VulkanCommandPoolWrapper::VulkanCommandPoolWrapper(
    std::unique_ptr<gpu::VulkanCommandPool> command_pool,
    std::unique_ptr<gpu::VulkanCommandBuffer> primary_command_buf)
    : command_pool_(std::move(command_pool)),
      primary_command_buf_(std::move(primary_command_buf)) {}

VulkanImageProcessorBackend::VulkanCommandPoolWrapper::
    ~VulkanCommandPoolWrapper() {
  primary_command_buf_->Destroy();
  primary_command_buf_ = nullptr;
  command_pool_->Destroy();
}

gpu::VulkanCommandBuffer* VulkanImageProcessorBackend::
    VulkanCommandPoolWrapper::GetPrimaryCommandBuffer() {
  return primary_command_buf_.get();
}

std::unique_ptr<VulkanImageProcessorBackend::VulkanCommandPoolWrapper>
VulkanImageProcessorBackend::VulkanCommandPoolWrapper::Create(
    gpu::VulkanDeviceQueue* device_queue) {
  std::unique_ptr<gpu::VulkanCommandPool> command_pool =
      base::WrapUnique(new gpu::VulkanCommandPool(device_queue));
  command_pool->Initialize();
  std::unique_ptr<gpu::VulkanCommandBuffer> primary_command_buf =
      command_pool->CreatePrimaryCommandBuffer();

  return base::WrapUnique(new VulkanCommandPoolWrapper(
      std::move(command_pool), std::move(primary_command_buf)));
}

VulkanImageProcessorBackend::VulkanImageProcessorBackend(
    gfx::Size input_size,
    gfx::Size output_size,
    std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation,
    std::unique_ptr<VulkanImageProcessorBackend::VulkanDeviceQueueWrapper>
        vulkan_device_queue,
    std::unique_ptr<VulkanImageProcessorBackend::VulkanCommandPoolWrapper>
        command_pool,
    std::unique_ptr<VulkanImageProcessorBackend::VulkanRenderPass> render_pass,
    std::unique_ptr<VulkanImageProcessorBackend::VulkanPipeline> pipeline,
    std::unique_ptr<VulkanImageProcessorBackend::VulkanDescriptorPool>
        descriptor_pool,
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    ErrorCB error_cb)
    : ImageProcessorBackend(input_config,
                            output_config,
                            output_mode,
                            std::move(error_cb),
                            base::ThreadPool::CreateSingleThreadTaskRunner(
                                {base::TaskPriority::USER_VISIBLE})),
      input_size_(input_size),
      output_size_(output_size),
      vulkan_implementation_(std::move(vulkan_implementation)),
      vulkan_device_queue_(std::move(vulkan_device_queue)),
      command_pool_(std::move(command_pool)),
      render_pass_(std::move(render_pass)),
      pipeline_(std::move(pipeline)),
      descriptor_pool_(std::move(descriptor_pool)) {}

VulkanImageProcessorBackend::~VulkanImageProcessorBackend() = default;

std::string VulkanImageProcessorBackend::type() const {
  return "VukanImageProcessor";
}

bool VulkanImageProcessorBackend::IsSupported(const PortConfig& input_config,
                                              const PortConfig& output_config,
                                              OutputMode output_mode) {
  if (output_mode != OutputMode::IMPORT) {
    return false;
  }

  if (!input_config.visible_rect.origin().IsOrigin() ||
      !output_config.visible_rect.origin().IsOrigin()) {
    VLOGF(2)
        << "The VulkanImageProcessorBackend does not support transposition.";
    return false;
  }

  if (input_config.fourcc != Fourcc(Fourcc::MM21) ||
      output_config.fourcc != Fourcc(Fourcc::AR24)) {
    VLOGF(2) << "The VulkanImageProcessorBackend only supports MM21->AR24 "
                "conversion.";
    return false;
  }

  // TODO(b/251458823): We want to support arbitrary scaling long term, this is
  // just a short term hack because we need to revamp our bilinear filtering
  // algorithm.
  if (output_config.visible_rect.width() <=
          input_config.visible_rect.width() / 2 ||
      output_config.visible_rect.height() <=
          input_config.visible_rect.height() / 2) {
    VLOGF(2) << "The VulkanImageProcessorBackend cannot downsample by more "
                "than 2 in each dimension.";
    return false;
  }

  return true;
}

std::unique_ptr<VulkanImageProcessorBackend>
VulkanImageProcessorBackend::Create(const PortConfig& input_config,
                                    const PortConfig& output_config,
                                    OutputMode output_mode,
                                    ErrorCB error_cb) {
  if (!IsSupported(input_config, output_config, output_mode)) {
    return nullptr;
  }

  auto vulkan_implementation = gpu::CreateVulkanImplementation(
      /*use_swiftshader=*/false, /*allow_protected_memory=*/false);
  vulkan_implementation->InitializeVulkanInstance(/*using_surface=*/false);

  auto vulkan_device_queue =
      VulkanDeviceQueueWrapper::Create(vulkan_implementation.get());
  if (!vulkan_device_queue) {
    return nullptr;
  }

  auto command_pool = VulkanCommandPoolWrapper::Create(
      vulkan_device_queue->GetVulkanDeviceQueue());
  if (!command_pool) {
    return nullptr;
  }

  auto render_pass = VulkanRenderPass::Create(
      VK_FORMAT_B8G8R8A8_UNORM, vulkan_device_queue->GetVulkanDevice());
  if (!render_pass) {
    return nullptr;
  }

  std::vector<VkVertexInputBindingDescription> binding_descriptions;
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

  std::vector<VkDescriptorSetLayoutBinding> descriptor_bindings(2);
  descriptor_bindings[0].binding = 0;
  descriptor_bindings[0].descriptorCount = 1;
  descriptor_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptor_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  descriptor_bindings[1].binding = 1;
  descriptor_bindings[1].descriptorCount = 1;
  descriptor_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptor_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  auto vert_shader =
      VulkanShader::Create(kMM21ShaderVert, sizeof(kMM21ShaderVert),
                           vulkan_device_queue->GetVulkanDevice());
  if (!vert_shader) {
    return nullptr;
  }
  auto frag_shader =
      VulkanShader::Create(kMM21ShaderFrag, sizeof(kMM21ShaderFrag),
                           vulkan_device_queue->GetVulkanDevice());
  if (!frag_shader) {
    return nullptr;
  }

  auto pipeline = VulkanPipeline::Create(
      binding_descriptions, attribute_descriptions, descriptor_bindings,
      std::move(vert_shader), std::move(frag_shader),
      output_config.visible_rect.size(), 2 * 2 * sizeof(int),
      render_pass->Get(), vulkan_device_queue->GetVulkanDevice());
  if (!pipeline) {
    return nullptr;
  }

  auto descriptor_pool = VulkanDescriptorPool::Create(
      1, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE},
      pipeline->GetDescriptorSetLayout(),
      vulkan_device_queue->GetVulkanDevice());
  if (!descriptor_pool) {
    return nullptr;
  }

  return base::WrapUnique(new VulkanImageProcessorBackend(
      input_config.size, output_config.visible_rect.size(),
      std::move(vulkan_implementation), std::move(vulkan_device_queue),
      std::move(command_pool), std::move(render_pass), std::move(pipeline),
      std::move(descriptor_pool), input_config, output_config, output_mode,
      std::move(error_cb)));
}

void VulkanImageProcessorBackend::Process(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    FrameReadyCB cb) {
  // Import output_frame into vulkan.
  gfx::GpuMemoryBufferHandle out_gmb =
      CreateGpuMemoryBufferHandle(output_frame.get());
  auto out_image = vulkan_implementation_->CreateImageFromGpuMemoryHandle(
      vulkan_device_queue_->GetVulkanDeviceQueue(), std::move(out_gmb),
      output_size_, VK_FORMAT_B8G8R8A8_UNORM,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                      gfx::ColorSpace::TransferID::BT709));
  auto out_texture = VulkanTextureImage::Create(
      std::move(out_image), {VK_FORMAT_B8G8R8A8_UNORM}, {output_size_},
      {VK_IMAGE_ASPECT_COLOR_BIT},
      /*is_framebuffer=*/true, render_pass_->Get(),
      vulkan_device_queue_->GetVulkanDevice());

  gfx::GpuMemoryBufferHandle in_gmb =
      CreateGpuMemoryBufferHandle(input_frame.get());
  auto in_image = vulkan_implementation_->CreateImageFromGpuMemoryHandle(
      vulkan_device_queue_->GetVulkanDeviceQueue(), std::move(in_gmb),
      input_size_, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                      gfx::ColorSpace::TransferID::BT709));
  gfx::Size uv_plane_size =
      gfx::Size((input_size_.width() + 1) / 2, (input_size_.height() + 1) / 2);
  auto in_texture = VulkanTextureImage::Create(
      std::move(in_image), {VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM},
      {input_size_, uv_plane_size},
      {VK_IMAGE_ASPECT_PLANE_0_BIT, VK_IMAGE_ASPECT_PLANE_1_BIT},
      /*is_framebuffer=*/false, render_pass_->Get(),
      vulkan_device_queue_->GetVulkanDevice());

  std::array<VkWriteDescriptorSet, 2> descriptor_write;

  std::array<VkDescriptorImageInfo, 2> image_info;
  image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info[0].imageView = in_texture->GetImageViews()[0];
  image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info[1].imageView = in_texture->GetImageViews()[1];
  descriptor_write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write[0].dstSet = descriptor_pool_->Get()[0];
  descriptor_write[0].dstBinding = 0;
  descriptor_write[0].dstArrayElement = 0;
  descriptor_write[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptor_write[0].descriptorCount = 1;
  descriptor_write[0].pImageInfo = image_info.data();
  descriptor_write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write[1].dstSet = descriptor_pool_->Get()[0];
  descriptor_write[1].dstBinding = 1;
  descriptor_write[1].dstArrayElement = 0;
  descriptor_write[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptor_write[1].descriptorCount = 1;
  descriptor_write[1].pImageInfo = image_info.data() + 1;

  vkUpdateDescriptorSets(vulkan_device_queue_->GetVulkanDevice(),
                         descriptor_write.size(), descriptor_write.data(), 0,
                         nullptr);

  {
    gpu::ScopedSingleUseCommandBufferRecorder record(
        *command_pool_->GetPrimaryCommandBuffer());

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(output_size_.width());
    viewport.height = static_cast<float>(output_size_.height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(record.handle(), 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent.width = static_cast<float>(output_size_.width());
    scissor.extent.height = static_cast<float>(output_size_.height());
    vkCmdSetScissor(record.handle(), 0, 1, &scissor);

    in_texture->TransitionImageLayout(command_pool_->GetPrimaryCommandBuffer(),
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass_->Get();
    render_pass_info.framebuffer = out_texture->GetFramebuffers()[0];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = {
        static_cast<uint32_t>(output_size_.width()),
        static_cast<uint32_t>(output_size_.height())};
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(record.handle(), &render_pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(record.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline_->Get());

    vkCmdBindDescriptorSets(record.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_->GetPipelineLayout(), 0, 1,
                            descriptor_pool_->Get().data(), 0, nullptr);

    int push_constants[4] = {input_config_.size.width(),
                             input_config_.size.height(),
                             input_config_.visible_rect.width(),
                             input_config_.visible_rect.height()};
    vkCmdPushConstants(record.handle(), pipeline_->GetPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants),
                       push_constants);

    vkCmdDraw(record.handle(), 6, 1, 0, 0);

    vkCmdEndRenderPass(record.handle());

    out_texture->TransitionImageLayout(
        command_pool_->GetPrimaryCommandBuffer(), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_FOREIGN_EXT);
  }
  command_pool_->GetPrimaryCommandBuffer()->Submit(0, nullptr, 0, nullptr);
  // TODO(b/251458823): We may not need to block here, see if we can use
  // a semaphore or a fence.
  command_pool_->GetPrimaryCommandBuffer()->Wait(UINT64_MAX);
  std::move(cb).Run(std::move(output_frame));
}

}  // namespace media
