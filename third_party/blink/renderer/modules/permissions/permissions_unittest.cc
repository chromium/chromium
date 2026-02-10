#include "third_party/blink/renderer/modules/permissions/permissions.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"

namespace blink {
namespace {

using mojom::blink::PermissionName;

mojom::blink::PermissionDescriptorPtr CreateDescriptor(PermissionName name) {
  auto descriptor = mojom::blink::PermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

mojom::blink::PermissionDescriptorPtr CreateMidiDescriptor(bool sysex) {
  auto descriptor = CreateDescriptor(PermissionName::MIDI);
  auto extension = mojom::blink::MidiPermissionDescriptor::New();
  extension->sysex = sysex;
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::NewMidi(
      std::move(extension));
  return descriptor;
}

TEST(PermissionsDescriptorEquivalenceTest, SameNameNoExtension) {
  auto left = CreateDescriptor(PermissionName::GEOLOCATION);
  auto right = CreateDescriptor(PermissionName::GEOLOCATION);
  EXPECT_TRUE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

TEST(PermissionsDescriptorEquivalenceTest, SameNameSameExtension) {
  auto left = CreateMidiDescriptor(/*sysex=*/true);
  auto right = CreateMidiDescriptor(/*sysex=*/true);
  EXPECT_TRUE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

TEST(PermissionsDescriptorEquivalenceTest, SameNameDifferentExtension) {
  auto left = CreateMidiDescriptor(/*sysex=*/true);
  auto right = CreateMidiDescriptor(/*sysex=*/false);
  EXPECT_FALSE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

TEST(PermissionsDescriptorEquivalenceTest, DifferentName) {
  auto left = CreateDescriptor(PermissionName::GEOLOCATION);
  auto right = CreateDescriptor(PermissionName::VIDEO_CAPTURE);
  EXPECT_FALSE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

TEST(PermissionsDescriptorEquivalenceTest, OneWithExtensionOneWithout) {
  auto left = CreateMidiDescriptor(/*sysex=*/true);
  auto right = CreateDescriptor(PermissionName::MIDI);
  EXPECT_FALSE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

}  // namespace
}  // namespace blink
